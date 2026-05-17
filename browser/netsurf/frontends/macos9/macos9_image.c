/* macos9_image.c -- NetSurf image content handler backed by QuickTime
 * Graphics Importers. fixes78.
 *
 * Architecture (fixes78j refactor):
 *
 *   process_data: accumulate raw bytes into a Mac Handle.
 *   data_complete: ask QT for an importer, query natural bounds, decode
 *                  ONCE into a 32-bit GWorld, byte-swap into a NetSurf
 *                  struct bitmap (RGBA), free importer + Handle + temp
 *                  GWorld.
 *   redraw:       one-liner into ctx->plot->bitmap (= macos9_plot_bitmap).
 *   destroy:      drop the bitmap via guit->bitmap->destroy.
 *
 * Why not call GraphicsImportDraw at redraw time? content_redraw for
 * images is dispatched outside the html_redraw box walker's plot_clip
 * scope, so a direct QT draw lands at an undefined clip / port state.
 * Routing through ctx->plot->bitmap puts the actual blit inside the
 * box walker's clip context (the path Atari / RISC OS / Amiga
 * frontends use), which is fixes77f's offscreen GWorld during paint.
 */

#include "macos9.h"

#include <Movies.h>

#include <stdlib.h>
#include <string.h>

#include "utils/errors.h"
#include "netsurf/types.h"
#include "netsurf/plotters.h"
#include "netsurf/content.h"
#include "netsurf/bitmap.h"
#include "content/content_protected.h"
#include "content/content_factory.h"
#include "desktop/gui_internal.h"
#include "desktop/gui_table.h"

#include "macsurf_debug.h"

#ifndef graphicsModeStraightAlpha
#define graphicsModeStraightAlpha 256
#endif

typedef ComponentInstance GraphicsImportComponent;

typedef struct macos9_qt_image_content {
	struct content base;     /* MUST be first */
	Handle compressed;       /* raw bytes, grown by process_data */
	void *bitmap;            /* struct macos9_bitmap (RGBA pixels) */
} macos9_qt_image_content;

static nserror
macos9_qt_image_create(const struct content_handler *handler,
		lwc_string *imime_type, const struct http_parameter *params,
		struct llcache_handle *llcache, const char *fallback_charset,
		bool quirks, struct content **c)
{
	macos9_qt_image_content *qti;
	nserror err;

	qti = (macos9_qt_image_content *)calloc(1,
			sizeof(macos9_qt_image_content));
	if (qti == NULL) {
		return NSERROR_NOMEM;
	}

	err = content__init(&qti->base, handler, imime_type, params,
			llcache, fallback_charset, quirks);
	if (err != NSERROR_OK) {
		free(qti);
		return err;
	}

	qti->compressed = NULL;
	qti->bitmap = NULL;

	*c = (struct content *)qti;
	return NSERROR_OK;
}

static bool
macos9_qt_image_process(struct content *c, const char *data,
		unsigned int size)
{
	macos9_qt_image_content *qti = (macos9_qt_image_content *)c;
	Size old_size;
	OSErr err;

	if (size == 0) {
		return true;
	}

	if (qti->compressed == NULL) {
		qti->compressed = NewHandle((Size)size);
		if (qti->compressed == NULL) {
			content_broadcast_error(c, NSERROR_NOMEM, NULL);
			return false;
		}
		old_size = 0;
	} else {
		old_size = GetHandleSize(qti->compressed);
		SetHandleSize(qti->compressed, old_size + (Size)size);
		err = MemError();
		if (err != noErr) {
			content_broadcast_error(c, NSERROR_NOMEM, NULL);
			return false;
		}
	}

	HLock(qti->compressed);
	BlockMoveData(data, (*qti->compressed) + old_size, (Size)size);
	HUnlock(qti->compressed);
	return true;
}

/* Sniff the compressed bytes for an alpha-capable format.
 *   PNG  -> 89 50 4E 47 0D 0A 1A 0A
 *   TIFF -> 49 49 2A 00 (LE) or 4D 4D 00 2A (BE)
 * GIF is excluded: QT's GIF importer doesn't respect
 * graphicsModeStraightAlpha and produces useless output through that
 * path. JPEG / 24-bit BMP have no alpha at file level. */
static bool
macos9_qt_format_has_alpha(const unsigned char *p, long n)
{
	if (n < 4) return false;
	if (p[0] == 0x89 && p[1] == 0x50 && p[2] == 0x4E && p[3] == 0x47) {
		return true;
	}
	if ((p[0] == 'I' && p[1] == 'I' && p[2] == 0x2A && p[3] == 0x00) ||
			(p[0] == 'M' && p[1] == 'M' &&
			 p[2] == 0x00 && p[3] == 0x2A)) {
		return true;
	}
	return false;
}

/* Sentinel for color-key transparency. PNG/TIFF transparent pixels
 * (alpha < 128) get rewritten to this RGB at decode time; the bitmap
 * plotter blits with `transparent` transfer mode + magenta bgColor so
 * matching pixels skip and the destination (card background) shows. */
#define MACOS9_IMG_TRANSPARENT_R 0xFF
#define MACOS9_IMG_TRANSPARENT_G 0x00
#define MACOS9_IMG_TRANSPARENT_B 0xFF

/* Helper: erase the given GWorld to the given RGB and decode the
 * importer into it. The current port is left set to gw on return. */
static void
macos9_qt_draw_into(GraphicsImportComponent gi, GWorldPtr gw,
		const Rect *r, unsigned short rr, unsigned short gg,
		unsigned short bb)
{
	RGBColor c;
	SetGWorld(gw, NULL);
	c.red = rr;
	c.green = gg;
	c.blue = bb;
	RGBBackColor(&c);
	EraseRect(r);
	GraphicsImportSetGWorld(gi, gw, NULL);
	GraphicsImportSetBoundsRect(gi, r);
	GraphicsImportDraw(gi);
}

/* Decode the importer into a freshly-allocated NetSurf bitmap.
 * Returns NSERROR_OK on success. On failure the bitmap is freed and
 * *out_bitmap is left NULL. */
static nserror
macos9_qt_decode_to_bitmap(GraphicsImportComponent gi,
		int bw, int bh, bool wants_alpha, void **out_bitmap)
{
	void *bm;
	unsigned char *dst_buf;
	GWorldPtr gw_a;
	GWorldPtr gw_b;
	Rect tmp_bounds;
	PixMapHandle pm_a;
	PixMapHandle pm_b;
	OSErr err;
	long row_bytes;
	long pa_rowbytes;
	long pb_rowbytes;
	int row, col;
	unsigned char *pa_base;
	unsigned char *pb_base;
	unsigned char *pa_row;
	unsigned char *pb_row;
	unsigned char *dst_row;
	CGrafPtr save_port;
	GDHandle save_gdh;

	*out_bitmap = NULL;

	if (guit == NULL || guit->bitmap == NULL ||
			guit->bitmap->create == NULL) {
		MS_LOG("img decode: guit bitmap NULL");
		return NSERROR_INIT_FAILED;
	}

	bm = guit->bitmap->create(bw, bh, BITMAP_CLEAR);
	if (bm == NULL) {
		MS_LOG("img decode: bitmap create FAIL");
		return NSERROR_NOMEM;
	}

	dst_buf = (unsigned char *)guit->bitmap->get_buffer(bm);
	row_bytes = (long)guit->bitmap->get_rowstride(bm);
	if (dst_buf == NULL || row_bytes <= 0) {
		MS_LOG("img decode: bitmap buf/stride bad");
		guit->bitmap->destroy(bm);
		return NSERROR_NOMEM;
	}

	tmp_bounds.left = 0;
	tmp_bounds.top = 0;
	tmp_bounds.right = (short)bw;
	tmp_bounds.bottom = (short)bh;

	gw_a = NULL;
	gw_b = NULL;
	pm_a = NULL;
	pm_b = NULL;

	/* useTempMem (= 4) so the temp GWorld doesn't exhaust the app
	 * heap for larger images. Fall back to app heap if temp fails. */
	err = NewGWorld(&gw_a, 32, &tmp_bounds, NULL, NULL,
			(GWorldFlags)4);
	if (err != noErr || gw_a == NULL) {
		err = NewGWorld(&gw_a, 32, &tmp_bounds, NULL, NULL, 0);
		if (err != noErr || gw_a == NULL) {
			MS_LOG("img decode: NewGWorld(A) FAIL");
			guit->bitmap->destroy(bm);
			return NSERROR_NOMEM;
		}
	}
	pm_a = GetGWorldPixMap(gw_a);
	if (pm_a == NULL || !LockPixels(pm_a)) {
		MS_LOG("img decode: LockPixels(A) FAIL");
		DisposeGWorld(gw_a);
		guit->bitmap->destroy(bm);
		return NSERROR_NOMEM;
	}
	pa_rowbytes = (*pm_a)->rowBytes & 0x3FFF;
	pa_base = (unsigned char *)GetPixBaseAddr(pm_a);

	GetGWorld(&save_port, &save_gdh);

	if (wants_alpha) {
		/* Two-pass alpha detection. Classic QuickDraw 32-bit
		 * GWorlds have no real alpha channel (the high byte is
		 * filler), so we can't read alpha from a single decoded
		 * pixmap. Instead, decode the image into TWO independent
		 * GWorlds primed with different background colors (white
		 * and black). Where the resulting RGB matches between the
		 * two, the pixel is fully opaque (background didn't bleed
		 * through). Where RGB differs, the source had alpha < 255
		 * -- those pixels become the magenta sentinel for
		 * color-key transparency at blit time.
		 *
		 * Two separate GWorlds (rather than reusing one) because
		 * GraphicsImportDraw's state-after-call on a single
		 * GWorld doesn't reliably re-decode on the next pass --
		 * earlier rounds with one GWorld produced all-magenta
		 * output even on opaque pixels. */
		err = NewGWorld(&gw_b, 32, &tmp_bounds, NULL, NULL,
				(GWorldFlags)4);
		if (err != noErr || gw_b == NULL) {
			err = NewGWorld(&gw_b, 32, &tmp_bounds, NULL, NULL, 0);
			if (err != noErr || gw_b == NULL) {
				MS_LOG("img decode: NewGWorld(B) FAIL");
				SetGWorld(save_port, save_gdh);
				UnlockPixels(pm_a);
				DisposeGWorld(gw_a);
				guit->bitmap->destroy(bm);
				return NSERROR_NOMEM;
			}
		}
		pm_b = GetGWorldPixMap(gw_b);
		if (pm_b == NULL || !LockPixels(pm_b)) {
			MS_LOG("img decode: LockPixels(B) FAIL");
			SetGWorld(save_port, save_gdh);
			UnlockPixels(pm_a);
			DisposeGWorld(gw_a);
			DisposeGWorld(gw_b);
			guit->bitmap->destroy(bm);
			return NSERROR_NOMEM;
		}
		pb_rowbytes = (*pm_b)->rowBytes & 0x3FFF;
		pb_base = (unsigned char *)GetPixBaseAddr(pm_b);

		/* Decode into gw_a primed white, gw_b primed black. */
		macos9_qt_draw_into(gi, gw_a, &tmp_bounds,
				0xFFFF, 0xFFFF, 0xFFFF);
		macos9_qt_draw_into(gi, gw_b, &tmp_bounds,
				0, 0, 0);

		for (row = 0; row < bh; row++) {
			pa_row = pa_base + row * pa_rowbytes;
			pb_row = pb_base + row * pb_rowbytes;
			dst_row = dst_buf + row * row_bytes;
			for (col = 0; col < bw; col++) {
				int dr, dg, db;
				dr = (int)pa_row[col * 4 + 1]
						- (int)pb_row[col * 4 + 1];
				dg = (int)pa_row[col * 4 + 2]
						- (int)pb_row[col * 4 + 2];
				db = (int)pa_row[col * 4 + 3]
						- (int)pb_row[col * 4 + 3];
				if (dr < 0) dr = -dr;
				if (dg < 0) dg = -dg;
				if (db < 0) db = -db;
				/* Threshold > 8 absorbs decoder jitter
				 * and slight bg-bleed at half-transparent
				 * edges while still catching real
				 * alpha-driven differences. */
				if (dr > 8 || dg > 8 || db > 8) {
					dst_row[col * 4 + 0] =
						MACOS9_IMG_TRANSPARENT_R;
					dst_row[col * 4 + 1] =
						MACOS9_IMG_TRANSPARENT_G;
					dst_row[col * 4 + 2] =
						MACOS9_IMG_TRANSPARENT_B;
					dst_row[col * 4 + 3] = 0xFF;
				} else {
					/* Opaque -- use the white-bg pass's
					 * RGB (matches black-bg's anyway). */
					dst_row[col * 4 + 0] =
							pa_row[col * 4 + 1];
					dst_row[col * 4 + 1] =
							pa_row[col * 4 + 2];
					dst_row[col * 4 + 2] =
							pa_row[col * 4 + 3];
					dst_row[col * 4 + 3] = 0xFF;
				}
			}
		}
	} else {
		/* Opaque-only formats: single decode on a white bg. */
		macos9_qt_draw_into(gi, gw_a, &tmp_bounds,
				0xFFFF, 0xFFFF, 0xFFFF);
		for (row = 0; row < bh; row++) {
			pa_row = pa_base + row * pa_rowbytes;
			dst_row = dst_buf + row * row_bytes;
			for (col = 0; col < bw; col++) {
				dst_row[col * 4 + 0] = pa_row[col * 4 + 1];
				dst_row[col * 4 + 1] = pa_row[col * 4 + 2];
				dst_row[col * 4 + 2] = pa_row[col * 4 + 3];
				dst_row[col * 4 + 3] = 0xFF;
			}
		}
	}

	SetGWorld(save_port, save_gdh);

	if (pm_b != NULL) UnlockPixels(pm_b);
	if (gw_b != NULL) DisposeGWorld(gw_b);
	UnlockPixels(pm_a);
	DisposeGWorld(gw_a);

	if (guit->bitmap->set_opaque != NULL) {
		guit->bitmap->set_opaque(bm, !wants_alpha);
	}
	if (guit->bitmap->modified != NULL) {
		guit->bitmap->modified(bm);
	}

	*out_bitmap = bm;
	return NSERROR_OK;
}

static bool
macos9_qt_image_convert(struct content *c)
{
	macos9_qt_image_content *qti = (macos9_qt_image_content *)c;
	Handle data_ref;
	OSErr osr;
	ComponentResult cr;
	GraphicsImportComponent importer;
	Rect bounds;
	int bw, bh;
	long src_size;
	bool wants_alpha;
	nserror err;

	if (qti->compressed == NULL ||
			GetHandleSize(qti->compressed) == 0) {
		MS_LOG("img convert: no bytes");
		content_broadcast_error(c, NSERROR_INVALID, NULL);
		return false;
	}

	src_size = GetHandleSize(qti->compressed);
	HLock(qti->compressed);
	wants_alpha = macos9_qt_format_has_alpha(
			(const unsigned char *)*qti->compressed, src_size);
	HUnlock(qti->compressed);
	MS_LOG(wants_alpha ? "img convert: alpha format" :
			"img convert: opaque format");

	data_ref = NULL;
	osr = PtrToHand(&qti->compressed, &data_ref, (long)sizeof(Handle));
	if (osr != noErr || data_ref == NULL) {
		MS_LOG("img convert: PtrToHand FAIL");
		content_broadcast_error(c, NSERROR_NOMEM, NULL);
		return false;
	}

	importer = NULL;
	cr = GetGraphicsImporterForDataRef(data_ref,
			HandleDataHandlerSubType, &importer);
	DisposeHandle(data_ref);
	if (cr != noErr || importer == NULL) {
		MS_LOG("img convert: no importer for data");
		content_broadcast_error(c, NSERROR_INVALID, NULL);
		return false;
	}

	bounds.top = 0;
	bounds.left = 0;
	bounds.bottom = 0;
	bounds.right = 0;
	cr = GraphicsImportGetNaturalBounds(importer, &bounds);
	if (cr != noErr) {
		MS_LOG("img convert: GetNaturalBounds FAIL");
		CloseComponent(importer);
		content_broadcast_error(c, NSERROR_INVALID, NULL);
		return false;
	}

	bw = (int)(bounds.right - bounds.left);
	bh = (int)(bounds.bottom - bounds.top);
	if (bw <= 0 || bh <= 0) {
		MS_LOG("img convert: bad bounds");
		CloseComponent(importer);
		content_broadcast_error(c, NSERROR_INVALID, NULL);
		return false;
	}
	macsurf_debug_log_writef("img convert: %dx%d %s, decoding",
			bw, bh, wants_alpha ? "alpha" : "opaque");

	err = macos9_qt_decode_to_bitmap(importer, bw, bh, wants_alpha,
			&qti->bitmap);
	CloseComponent(importer);
	if (qti->compressed != NULL) {
		DisposeHandle(qti->compressed);
		qti->compressed = NULL;
	}

	if (err != NSERROR_OK || qti->bitmap == NULL) {
		MS_LOG("img convert: decode FAIL");
		content_broadcast_error(c, err, NULL);
		return false;
	}

	c->width = bw;
	c->height = bh;

	MS_LOG("img decoded to bitmap");
	content_set_ready(c);
	content_set_done(c);
	content_set_status(c, "");
	return true;
}

static bool
macos9_qt_image_redraw(struct content *c, struct content_redraw_data *data,
		const struct rect *clip, const struct redraw_context *ctx)
{
	macos9_qt_image_content *qti = (macos9_qt_image_content *)c;
	bitmap_flags_t flags;

	(void)clip;

	if (qti->bitmap == NULL || ctx == NULL ||
			ctx->plot == NULL || ctx->plot->bitmap == NULL) {
		return true;
	}

	flags = 0;
	if (data->repeat_x) flags |= BITMAPF_REPEAT_X;
	if (data->repeat_y) flags |= BITMAPF_REPEAT_Y;

	return ctx->plot->bitmap(ctx, (struct bitmap *)qti->bitmap,
			data->x, data->y,
			data->width, data->height,
			data->background_colour,
			flags) == NSERROR_OK;
}

static void
macos9_qt_image_destroy(struct content *c)
{
	macos9_qt_image_content *qti = (macos9_qt_image_content *)c;

	if (qti->bitmap != NULL) {
		if (guit != NULL && guit->bitmap != NULL &&
				guit->bitmap->destroy != NULL) {
			guit->bitmap->destroy(qti->bitmap);
		}
		qti->bitmap = NULL;
	}
	if (qti->compressed != NULL) {
		DisposeHandle(qti->compressed);
		qti->compressed = NULL;
	}
}

static nserror
macos9_qt_image_clone(const struct content *old, struct content **newc)
{
	(void)old;
	(void)newc;
	return NSERROR_CLONE_FAILED;
}

static content_type
macos9_qt_image_type(void)
{
	return CONTENT_IMAGE;
}

static bool
macos9_qt_image_is_opaque(struct content *c)
{
	macos9_qt_image_content *qti = (macos9_qt_image_content *)c;
	if (qti->bitmap != NULL && guit != NULL && guit->bitmap != NULL &&
			guit->bitmap->get_opaque != NULL) {
		return guit->bitmap->get_opaque(qti->bitmap);
	}
	return false;
}

/* Vtable -- positional init (CW8 C89 has no designated initialisers).
 * Field order MUST match struct content_handler in
 * content/content_protected.h. */
static const struct content_handler macos9_qt_image_handler = {
	NULL,                          /* fini */
	macos9_qt_image_create,        /* create */
	macos9_qt_image_process,       /* process_data */
	macos9_qt_image_convert,       /* data_complete */
	NULL,                          /* reformat */
	macos9_qt_image_destroy,       /* destroy */
	NULL,                          /* stop */
	NULL,                          /* mouse_track */
	NULL,                          /* mouse_action */
	NULL,                          /* keypress */
	macos9_qt_image_redraw,        /* redraw */
	NULL,                          /* open */
	NULL,                          /* close */
	NULL,                          /* clear_selection */
	NULL,                          /* get_selection */
	NULL,                          /* get_contextual_content */
	NULL,                          /* scroll_at_point */
	NULL,                          /* drop_file_at_point */
	NULL,                          /* debug_dump */
	NULL,                          /* debug */
	macos9_qt_image_clone,         /* clone */
	NULL,                          /* matches_quirks */
	NULL,                          /* get_encoding */
	macos9_qt_image_type,          /* type */
	NULL,                          /* add_user */
	NULL,                          /* remove_user */
	NULL,                          /* exec */
	NULL,                          /* saw_insecure_objects */
	NULL,                          /* textsearch_find */
	NULL,                          /* textsearch_bounds */
	NULL,                          /* textselection_redraw */
	NULL,                          /* textselection_copy */
	NULL,                          /* textselection_get_end */
	NULL,                          /* get_internal */
	macos9_qt_image_is_opaque,     /* is_opaque */
	false                          /* no_share */
};

static const char *macos9_qt_image_mime[] = {
	"image/jpeg",
	"image/jpg",
	"image/pjpeg",
	"image/png",
	"image/x-png",
	"image/gif",
	"image/bmp",
	"image/x-bmp",
	"image/x-ms-bmp",
	"image/tiff",
	"image/x-tiff"
};

nserror image_init(void)
{
	nserror err;
	size_t i;
	size_t n;

	n = sizeof(macos9_qt_image_mime) / sizeof(macos9_qt_image_mime[0]);
	for (i = 0; i < n; ++i) {
		err = content_factory_register_handler(macos9_qt_image_mime[i],
				&macos9_qt_image_handler);
		if (err != NSERROR_OK) {
			return err;
		}
	}
	return NSERROR_OK;
}
