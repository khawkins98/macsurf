#include <stdio.h>
#include <time.h>
#include <string.h>
#include "macos9.h"
#include "duktape.h"
#include "macsurf_debug.h"
#include "macsurf_js.h"

#ifdef WITH_DUKTAPE

struct jsheap {
	duk_context *ctx;
};

struct jsthread {
	struct jsheap *heap;
	duk_context *ctx;
};

static struct jsheap *global_heap = NULL;

/* fixes288 (#115): console.log/warn/error/info/debug share one C function;
 * the level prefix tags the log line so post-mortem readers can tell which
 * stream the script wrote to. */
static void console_emit(duk_context *ctx, const char *prefix)
{
	int n = duk_get_top(ctx), i; char log_buf[512]; size_t pos = 0;
	size_t plen = strlen(prefix);
	log_buf[0] = '\0';
	if (plen < 510) { memcpy(log_buf, prefix, plen); pos = plen; }
	for (i = 0; i < n; i++) {
		const char *s = duk_safe_to_string(ctx, i); size_t slen = strlen(s);
		if (pos + slen + 2 < 512) {
			if (pos > plen) log_buf[pos++] = ' ';
			memcpy(log_buf + pos, s, slen); pos += slen;
		}
	}
	log_buf[pos] = '\0'; MS_LOG(log_buf);
}
static duk_ret_t native_console_log(duk_context *ctx) { console_emit(ctx, "[log] "); return 0; }
static duk_ret_t native_console_warn(duk_context *ctx) { console_emit(ctx, "[warn] "); return 0; }
static duk_ret_t native_console_error(duk_context *ctx) { console_emit(ctx, "[error] "); return 0; }
static duk_ret_t native_console_info(duk_context *ctx) { console_emit(ctx, "[info] "); return 0; }
static duk_ret_t native_console_debug(duk_context *ctx) { console_emit(ctx, "[debug] "); return 0; }

/* fixes288 (#121): atob / btoa.
 * btoa: ASCII string in -> base64 string out.
 * atob: base64 string in -> ASCII string out.
 * Per HTML spec, both operate on byte strings; we treat input as Latin-1.
 * Throws TypeError on invalid base64 input (matches browser behaviour). */
static const char b64_alphabet[] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static duk_ret_t native_btoa(duk_context *ctx)
{
	duk_size_t in_len = 0;
	const char *in = duk_require_lstring(ctx, 0, &in_len);
	duk_size_t out_cap = ((in_len + 2) / 3) * 4 + 1;
	char *out = (char *)malloc(out_cap);
	duk_size_t i = 0, o = 0;
	if (out == NULL) return DUK_RET_ERROR;
	while (i + 3 <= in_len) {
		unsigned int b0 = (unsigned char)in[i];
		unsigned int b1 = (unsigned char)in[i + 1];
		unsigned int b2 = (unsigned char)in[i + 2];
		out[o++] = b64_alphabet[(b0 >> 2) & 0x3F];
		out[o++] = b64_alphabet[((b0 << 4) | (b1 >> 4)) & 0x3F];
		out[o++] = b64_alphabet[((b1 << 2) | (b2 >> 6)) & 0x3F];
		out[o++] = b64_alphabet[b2 & 0x3F];
		i += 3;
	}
	if (i < in_len) {
		unsigned int b0 = (unsigned char)in[i];
		unsigned int b1 = (i + 1 < in_len) ? (unsigned char)in[i + 1] : 0;
		out[o++] = b64_alphabet[(b0 >> 2) & 0x3F];
		out[o++] = b64_alphabet[((b0 << 4) | (b1 >> 4)) & 0x3F];
		out[o++] = (i + 1 < in_len) ? b64_alphabet[(b1 << 2) & 0x3F] : '=';
		out[o++] = '=';
	}
	out[o] = '\0';
	duk_push_lstring(ctx, out, o);
	free(out);
	return 1;
}

static int b64_decode_char(int c)
{
	if (c >= 'A' && c <= 'Z') return c - 'A';
	if (c >= 'a' && c <= 'z') return c - 'a' + 26;
	if (c >= '0' && c <= '9') return c - '0' + 52;
	if (c == '+') return 62;
	if (c == '/') return 63;
	return -1;
}

static duk_ret_t native_atob(duk_context *ctx)
{
	duk_size_t in_len = 0;
	const char *in = duk_require_lstring(ctx, 0, &in_len);
	char *out;
	duk_size_t out_cap;
	duk_size_t i = 0, o = 0;
	int quad[4]; int qpos = 0;
	out_cap = (in_len / 4) * 3 + 3;
	out = (char *)malloc(out_cap);
	if (out == NULL) return DUK_RET_ERROR;
	for (i = 0; i < in_len; i++) {
		int c = (unsigned char)in[i];
		int v;
		if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f') continue;
		if (c == '=') { quad[qpos++] = -2; }
		else {
			v = b64_decode_char(c);
			if (v < 0) { free(out); return DUK_RET_TYPE_ERROR; }
			quad[qpos++] = v;
		}
		if (qpos == 4) {
			if (quad[0] < 0 || quad[1] < 0) { free(out); return DUK_RET_TYPE_ERROR; }
			out[o++] = (char)((quad[0] << 2) | (quad[1] >> 4));
			if (quad[2] >= 0) out[o++] = (char)((quad[1] << 4) | (quad[2] >> 2));
			if (quad[3] >= 0) out[o++] = (char)((quad[2] << 6) | quad[3]);
			qpos = 0;
		}
	}
	if (qpos != 0 && qpos != 4) { free(out); return DUK_RET_TYPE_ERROR; }
	duk_push_lstring(ctx, out, o);
	free(out);
	return 1;
}

static void register_console(duk_context *ctx)
{
	duk_push_global_object(ctx);
	duk_push_object(ctx);
	duk_push_c_function(ctx, native_console_log, DUK_VARARGS);
	duk_put_prop_string(ctx, -2, "log");
	duk_push_c_function(ctx, native_console_warn, DUK_VARARGS);
	duk_put_prop_string(ctx, -2, "warn");
	duk_push_c_function(ctx, native_console_error, DUK_VARARGS);
	duk_put_prop_string(ctx, -2, "error");
	duk_push_c_function(ctx, native_console_info, DUK_VARARGS);
	duk_put_prop_string(ctx, -2, "info");
	duk_push_c_function(ctx, native_console_debug, DUK_VARARGS);
	duk_put_prop_string(ctx, -2, "debug");
	duk_put_prop_string(ctx, -2, "console");
	duk_pop(ctx);
}

/* fixes288 (#120): navigator object.  Static read-only strings identifying
 * MacSurf as Mozilla-class so feature-sniffers don't bail on unrecognised UA.
 * Real-world UA strings on OS 9 PPC are rare enough that we pick something
 * close to what Classilla / iCab used to advertise. */
static void register_navigator(duk_context *ctx)
{
	duk_push_global_object(ctx);
	duk_push_object(ctx);
	duk_push_string(ctx,
		"Mozilla/4.0 (compatible; MacSurf; PPC Mac OS 9)");
	duk_put_prop_string(ctx, -2, "userAgent");
	duk_push_string(ctx, "4.0 (compatible; MacSurf; PPC Mac OS 9)");
	duk_put_prop_string(ctx, -2, "appVersion");
	duk_push_string(ctx, "MacPPC");
	duk_put_prop_string(ctx, -2, "platform");
	duk_push_string(ctx, "Netscape");
	duk_put_prop_string(ctx, -2, "appName");
	duk_push_string(ctx, "");
	duk_put_prop_string(ctx, -2, "vendor");
	duk_push_string(ctx, "en-US");
	duk_put_prop_string(ctx, -2, "language");
	duk_push_boolean(ctx, 1);
	duk_put_prop_string(ctx, -2, "onLine");
	duk_push_boolean(ctx, 0);
	duk_put_prop_string(ctx, -2, "cookieEnabled");
	duk_put_prop_string(ctx, -2, "navigator");
	duk_pop(ctx);
}

static void register_base64(duk_context *ctx)
{
	duk_push_global_object(ctx);
	duk_push_c_function(ctx, native_btoa, 1);
	duk_put_prop_string(ctx, -2, "btoa");
	duk_push_c_function(ctx, native_atob, 1);
	duk_put_prop_string(ctx, -2, "atob");
	duk_pop(ctx);
}

struct jsheap *js_new_heap(void) {
	struct jsheap *heap = (struct jsheap *)calloc(1, sizeof(*heap)); if (!heap) return NULL;
	heap->ctx = duk_create_heap_default(); if (!heap->ctx) { free(heap); return NULL; }
	register_console(heap->ctx);
	register_navigator(heap->ctx);
	register_base64(heap->ctx);
	global_heap = heap; return heap;
}
void js_destroy_heap(struct jsheap *heap) { if (!heap) return; if (heap->ctx) duk_destroy_heap(heap->ctx); if (global_heap == heap) global_heap = NULL; free(heap); }
struct jsthread *js_new_thread(struct jsheap *heap) {
	struct jsthread *thread = (struct jsthread *)calloc(1, sizeof(*thread)); if (!thread) return NULL;
	thread->heap = heap; thread->ctx = heap->ctx; return thread;
}
void js_destroy_thread(struct jsthread *thread) { free(thread); }

/* js_exec is provided by js_stub.c (NetSurf-compatible no-op stub) regardless
 * of WITH_DUKTAPE.  Real Duktape evaluation goes through macsurf_js_exec()
 * declared in macsurf_js.h, which has the correct internal signature. */

bool macsurf_js_exec_script(struct jsthread *thread, const char *txt, size_t len) {
	if (!thread || !txt) return (bool)0;
	duk_push_lstring(thread->ctx, txt, len);
	if (duk_peval(thread->ctx) != 0) { MS_LOG(duk_safe_to_string(thread->ctx, -1)); duk_pop(thread->ctx); return (bool)0; }
	duk_pop(thread->ctx); return (bool)1;
}
#endif
