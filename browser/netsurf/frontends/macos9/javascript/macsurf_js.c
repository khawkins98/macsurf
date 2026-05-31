/*
 * MacSurf — macsurf_js.c
 *
 * Duktape integration for the NetSurf js_thread API.
 *
 * fixes316 (#115/#116/#117/#118/#119/#120/#121/#123/#124/#125 unblock):
 * route NetSurf core's <script>-tag dispatch (js_newheap / js_newthread /
 * js_exec / ...) into Duktape. Pre-fixes316 these names resolved to the
 * no-op stubs in js_stub.c — Duktape was linked into the binary but
 * never reached because the MacSurf-side wrapper exported js_new_heap
 * (with an underscore) and an internal macsurf_js_exec_script, neither
 * of which NetSurf core ever called. Result: every <script> tag returned
 * 0 and the script body leaked through as visible text on the page.
 *
 * fixes316 retries the bridge from fixes289 ISOLATED — no bundled
 * Tier 1 globals (navigator / atob / encodeURI / alert / location /
 * history / document.title / setTimeout / fetch / etc.). Tier 1 stacks
 * one issue at a time once mactrove.com is regression-confirmed on the
 * bridge alone, per DIRECTIVE #5.
 *
 * Preserves the pre-existing in-heap console.log registration (it lived
 * inside the old js_new_heap as part of the recovery sprint and predates
 * the fixes282-289c chain that fixes290 reverted). console.log is
 * effectively dead pre-fix316 because nothing can reach it; post-fix316
 * the same code path becomes live. That's not a Tier 1 add — it's the
 * bridge waking up code that was already in the tree.
 */

#include <stdio.h>
#include <time.h>
#include <string.h>

#include "utils/ns_errors.h"
#include "macos9.h"
#include "duktape.h"
#include "macsurf_debug.h"
#include "macsurf_js.h"

#ifdef WITH_DUKTAPE

/* Forward declarations for the NetSurf DOM types so js_fire_event /
 * js_handle_new_element / js_event_cleanup can take them by pointer
 * without dragging in libdom headers (which need their own port pass). */
struct dom_event;
struct dom_document;
struct dom_node;
struct dom_element;
struct dom_string;

struct jsheap {
	duk_context *ctx;
	int timeout;
};

struct jsthread {
	struct jsheap *heap;
	duk_context *ctx;
	void *win_priv;
	void *doc_priv;
};

static struct jsheap *global_heap = NULL;

/* fixes319 (#115) — console.{log,warn,error,info,debug}.
 * All five route to MS_LOG with a level-tagged prefix. Real browsers
 * differentiate streams; on OS 9 we only have one log channel, so the
 * level shows up as a `[js:warn]` etc. prefix instead.
 *
 * Shared formatter: space-separated stringification, 512-byte cap. */
static void macsurf_js__console_emit(duk_context *ctx, const char *prefix)
{
	int n = duk_get_top(ctx);
	int i;
	char log_buf[512];
	size_t pos = 0;
	size_t prefix_len = strlen(prefix);
	log_buf[0] = '\0';
	if (prefix_len + 1 < 512) {
		memcpy(log_buf, prefix, prefix_len);
		pos = prefix_len;
	}
	for (i = 0; i < n; i++) {
		const char *s = duk_safe_to_string(ctx, i);
		size_t slen = strlen(s);
		if (pos + slen + 2 < 512) {
			if (i > 0 || prefix_len > 0) log_buf[pos++] = ' ';
			memcpy(log_buf + pos, s, slen);
			pos += slen;
		}
	}
	log_buf[pos] = '\0';
	MS_LOG(log_buf);
}

static duk_ret_t native_console_log(duk_context *ctx)
{
	macsurf_js__console_emit(ctx, "[js]");
	return 0;
}

static duk_ret_t native_console_warn(duk_context *ctx)
{
	macsurf_js__console_emit(ctx, "[js:warn]");
	return 0;
}

static duk_ret_t native_console_error(duk_context *ctx)
{
	macsurf_js__console_emit(ctx, "[js:error]");
	return 0;
}

static duk_ret_t native_console_info(duk_context *ctx)
{
	macsurf_js__console_emit(ctx, "[js:info]");
	return 0;
}

static duk_ret_t native_console_debug(duk_context *ctx)
{
	macsurf_js__console_emit(ctx, "[js:debug]");
	return 0;
}

/* fixes319 (#116) — alert / confirm / prompt via Carbon StandardAlert.
 * V1 behaviour:
 *   - alert(msg)      → StandardAlert(kAlertNoteAlert, ...), blocks
 *   - confirm(msg)    → StandardAlert with OK/Cancel buttons; returns
 *                       true if user clicked OK, false otherwise.
 *   - prompt(msg, def) → V1 returns the default (or null if none).
 *                       Real text-input prompt needs a custom DLOG
 *                       resource + ModalDialog plumbing; deferred.
 *
 * String input is converted to MacRoman (no UTF-8 in StandardAlert).
 * Length is clamped to 255 chars (Pascal string limit). */
#ifdef __MACOS9__
extern size_t macos9_utf8_to_macroman(const char *utf8, size_t len,
		char *mac_out, size_t max_out);

static void macsurf_js__cstr_to_pstr(const char *src, unsigned char *dst,
		size_t cap)
{
	size_t len = strlen(src);
	if (len > 255) len = 255;
	if (len > cap - 1) len = cap - 1;
	dst[0] = (unsigned char)len;
	{
		size_t i;
		for (i = 0; i < len; i++) dst[1 + i] = (unsigned char)src[i];
	}
}
#endif

static duk_ret_t native_alert(duk_context *ctx)
{
#ifdef __MACOS9__
	const char *msg = duk_safe_to_string(ctx, 0);
	char macroman[256];
	unsigned char pstr[256];
	short item;
	macos9_utf8_to_macroman(msg, strlen(msg), macroman, sizeof macroman);
	macsurf_js__cstr_to_pstr(macroman, pstr, sizeof pstr);
	StandardAlert(kAlertNoteAlert, pstr, "\p", NULL, &item);
#else
	const char *msg = duk_safe_to_string(ctx, 0);
	MS_LOG(msg);
#endif
	return 0;
}

static duk_ret_t native_confirm(duk_context *ctx)
{
#ifdef __MACOS9__
	const char *msg = duk_safe_to_string(ctx, 0);
	char macroman[256];
	unsigned char pstr[256];
	short item;
	/* fixes320e (#116) — pass an AlertStdAlertParamRec so the dialog
	 * actually shows OK + Cancel. Without params, StandardAlert uses
	 * defaults which is OK-only for both Note and Caution kinds. */
	AlertStdAlertParamRec params;
	macos9_utf8_to_macroman(msg, strlen(msg), macroman, sizeof macroman);
	macsurf_js__cstr_to_pstr(macroman, pstr, sizeof pstr);
	params.movable      = false;
	params.helpButton   = false;
	params.filterProc   = NULL;
	params.defaultText  = (StringPtr)"\pOK";
	params.cancelText   = (StringPtr)"\pCancel";
	params.otherText    = NULL;
	params.defaultButton = kAlertStdAlertOKButton;
	params.cancelButton  = kAlertStdAlertCancelButton;
	params.position     = kWindowDefaultPosition;
	StandardAlert(kAlertCautionAlert, pstr, "\p", &params, &item);
	/* Item 1 = OK, 2 = Cancel. */
	duk_push_boolean(ctx, item == kAlertStdAlertOKButton ? 1 : 0);
#else
	(void)ctx;
	duk_push_boolean(ctx, 0);
#endif
	return 1;
}

static duk_ret_t native_prompt(duk_context *ctx)
{
	/* V1: return the default value if supplied, else null. Full text
	 * input prompt deferred to a later round (needs DLOG resource +
	 * TextEdit + ModalDialog). */
	if (duk_get_top(ctx) >= 2 && !duk_is_undefined(ctx, 1)) {
		duk_dup(ctx, 1);
	} else {
		duk_push_null(ctx);
	}
	return 1;
}

/* fixes319 (#121) — atob / btoa via Duktape's duk_base64_*.
 * encodeURI / decodeURI / encodeURIComponent / decodeURIComponent are
 * provided by Duktape natively (DUK_USE_ENCODING_BUILTINS=1 in
 * duk_config.h). */
static duk_ret_t native_btoa(duk_context *ctx)
{
	duk_base64_encode(ctx, 0);
	return 1;
}

static duk_ret_t native_atob(duk_context *ctx)
{
	duk_base64_decode(ctx, 0);
	/* duk_base64_decode leaves a buffer on the stack; convert to
	 * binary string for compatibility with the HTML5 atob spec. */
	{
		duk_size_t blen = 0;
		const char *bytes = (const char *)duk_get_buffer(ctx, -1,
				&blen);
		if (bytes != NULL) {
			duk_push_lstring(ctx, bytes, blen);
			duk_remove(ctx, -2);
		}
	}
	return 1;
}

/* fixes319 (#120) — navigator object with userAgent, appVersion,
 * platform. Tracks the User-Agent string emitted by the HTTPS fetcher
 * so author scripts that sniff by UA see the same identity the network
 * layer is sending.
 *
 * MacSurf's UA is `MacSurf/<ver> (Macintosh; PPC Mac OS 9)` per the v1.x
 * release notes. Hardcoded here; future rounds may parameterise. */
#define MACSURF_JS_UA      "MacSurf/1.3 (Macintosh; PPC Mac OS 9)"
#define MACSURF_JS_PLATFORM "MacPPC"
#define MACSURF_JS_APPVER   "5.0 (Macintosh; PPC Mac OS 9)"

/* fixes319 (#119) — document.title getter / setter.
 * Getter reads through the active gui_window's title via NetSurf's
 * content_get_title; setter calls macos9_gw_set_title. The
 * document object lives elsewhere (macsurf_js_dom.c) and may already
 * have a .title slot; we hook into the global `document` if one is
 * present, otherwise we create a minimal one. */
#ifdef __MACOS9__
extern struct gui_window *initial_win;
extern void macos9_gw_set_title(struct gui_window *gw, const char *title);
#endif

static duk_ret_t native_document_title_get(duk_context *ctx)
{
	/* MVP: return empty string. Wiring through to NetSurf's
	 * content_get_title requires a per-thread browser-window pointer
	 * which we don't currently stash on the duk_context. The setter
	 * (below) does work via initial_win. Improving the getter is a
	 * follow-up. */
	duk_push_string(ctx, "");
	return 1;
}

static duk_ret_t native_document_title_set(duk_context *ctx)
{
#ifdef __MACOS9__
	const char *title = duk_safe_to_string(ctx, 0);
	if (initial_win != NULL && title != NULL) {
		macos9_gw_set_title(initial_win, title);
	}
#else
	(void)ctx;
#endif
	return 0;
}

/* Master registration: install all globals onto the heap's context.
 * Replaces the original register_console_log. */
static void register_browser_globals(duk_context *ctx)
{
	duk_push_global_object(ctx);

	/* console */
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

	/* alert / confirm / prompt */
	duk_push_c_function(ctx, native_alert, 1);
	duk_put_prop_string(ctx, -2, "alert");
	duk_push_c_function(ctx, native_confirm, 1);
	duk_put_prop_string(ctx, -2, "confirm");
	duk_push_c_function(ctx, native_prompt, 2);
	duk_put_prop_string(ctx, -2, "prompt");

	/* atob / btoa (encodeURI etc. are Duktape built-ins) */
	duk_push_c_function(ctx, native_btoa, 1);
	duk_put_prop_string(ctx, -2, "btoa");
	duk_push_c_function(ctx, native_atob, 1);
	duk_put_prop_string(ctx, -2, "atob");

	/* navigator */
	duk_push_object(ctx);
	duk_push_string(ctx, MACSURF_JS_UA);
	duk_put_prop_string(ctx, -2, "userAgent");
	duk_push_string(ctx, MACSURF_JS_APPVER);
	duk_put_prop_string(ctx, -2, "appVersion");
	duk_push_string(ctx, MACSURF_JS_PLATFORM);
	duk_put_prop_string(ctx, -2, "platform");
	duk_push_string(ctx, "Netscape");
	duk_put_prop_string(ctx, -2, "appName");
	duk_push_string(ctx, "en-US");
	duk_put_prop_string(ctx, -2, "language");
	duk_put_prop_string(ctx, -2, "navigator");

	/* fixes320e — document.title is now owned by macsurf_document_set_title
	 * in macsurf_js_dom.c (fixes320c wired it through macos9_gw_set_title,
	 * fixes320d added logging). Previously we tried to override the
	 * accessor here, which silently overrode the macsurf_document_set_title
	 * installed by macsurf_js_setup_globals — but our redefine call
	 * had no logging and used different DUK_DEFPROP flags, so the title
	 * setter that actually ran wasn't doing the chrome update. Drop the
	 * override and let setup_globals' setter run as-is. */

	duk_pop(ctx); /* pop global */
}

/* ------------------------------------------------------------------ */
/* fixes316 — NetSurf-API surface.                                    */
/* These names are the actual symbols NetSurf core links against from */
/* desktop/browser_window.c, content/handlers/html/script.c, etc.     */
/* Pre-fix316 these resolved to no-ops in js_stub.c; with the !WITH_  */
/* DUKTAPE gate in that file, the symbols now belong to us.           */
/* ------------------------------------------------------------------ */

/* fixes319b — minimal content_handler so the factory recognises
 * `text/javascript` (and friends) as CONTENT_JS. Without this,
 * select_script_handler() in script.c returns NULL on every <script>
 * tag and js_exec is never called. The official path in upstream
 * NetSurf is dukky.c → javascript_init() → CONTENT_FACTORY_REGISTER_TYPES
 * macro in js_content.c, but neither file is in MacSurf.mcp — that
 * was the missing link. We register the same MIME types here with a
 * stub handler whose only job is to make `type()` return CONTENT_JS;
 * inline scripts then route through exec_inline_script which reads
 * the source straight from the dom_string and never touches the
 * content_handler's other callbacks. */

#include "content/content_factory.h"
#include "content/content_protected.h"

static nserror macsurf_js__content_create(const struct content_handler *handler,
		lwc_string *imime_type,
		const struct http_parameter *params,
		struct llcache_handle *llcache, const char *fallback_charset,
		bool quirks, struct content **c)
{
	struct content *content;
	nserror error;
	(void)params;
	content = (struct content *)calloc(1, sizeof(struct content));
	if (content == NULL) return NSERROR_NOMEM;
	error = content__init(content, handler, imime_type, NULL, llcache,
			fallback_charset, quirks);
	if (error != NSERROR_OK) {
		free(content);
		return error;
	}
	*c = content;
	return NSERROR_OK;
}

static bool macsurf_js__content_convert(struct content *c)
{
	content_set_ready(c);
	content_set_done(c);
	return true;
}

static void macsurf_js__content_destroy(struct content *c)
{
	(void)c;
}

static nserror macsurf_js__content_clone(const struct content *old,
		struct content **newc)
{
	(void)old; (void)newc;
	return NSERROR_CLONE_FAILED;
}

static content_type macsurf_js__content_type(void)
{
	return CONTENT_JS;
}

static const struct content_handler macsurf_js__content_handler = {
	.create = macsurf_js__content_create,
	.data_complete = macsurf_js__content_convert,
	.destroy = macsurf_js__content_destroy,
	.clone = macsurf_js__content_clone,
	.type = macsurf_js__content_type,
	.no_share = false
};

static const char * const macsurf_js__content_types[] = {
	"application/javascript",
	"application/ecmascript",
	"application/x-javascript",
	"text/javascript",
	"text/ecmascript"
};

/* fixes319g — link-stub for macsurf_js_dom.c's macsurf_console_log.
 * The DOM file's pre-existing single-stream console.log routes through
 * macsurf_js_console_append, which was never defined anywhere. Now
 * that fixes319f calls macsurf_js_setup_globals (which references
 * macsurf_console_log), the dead symbol becomes a live link error.
 *
 * In practice macsurf_console_log is overwritten by my register_
 * browser_globals' level-distinguished console.* bindings, so it never
 * runs at runtime — but the symbol still has to link. Route to MS_LOG
 * with a generic prefix as a safety net. */
void macsurf_js_console_append(const char *line)
{
	MS_LOG(line ? line : "");
}

void js_initialise(void)
{
	size_t i;
	MS_LOG("js: initialise");
	for (i = 0; i < sizeof macsurf_js__content_types /
			sizeof macsurf_js__content_types[0]; i++) {
		nserror e = content_factory_register_handler(
			macsurf_js__content_types[i],
			&macsurf_js__content_handler);
		if (e != NSERROR_OK) {
			macsurf_debug_log_writef(
				"js: register %s failed err=%d",
				macsurf_js__content_types[i], (int)e);
		}
	}
	MS_LOG("js: content types registered");
}

void js_finalise(void)
{
	MS_LOG("js: finalise");
}

nserror js_newheap(int timeout, struct jsheap **out_heap)
{
	struct jsheap *heap;
	if (out_heap == NULL) return NSERROR_BAD_PARAMETER;
	*out_heap = NULL;
	heap = (struct jsheap *)calloc(1, sizeof(*heap));
	if (heap == NULL) return NSERROR_NOMEM;
	heap->ctx = duk_create_heap_default();
	if (heap->ctx == NULL) {
		free(heap);
		return NSERROR_NOMEM;
	}
	heap->timeout = timeout;
	/* fixes319f — call the DOM-bindings setup function FIRST so
	 * document.getElementById / querySelector / createElement are
	 * available. Previously it was dead code (declared in the .h,
	 * defined in macsurf_js_dom.c, never invoked anywhere). After
	 * setup_globals runs, register_browser_globals overlays the
	 * level-distinguished console.* plus navigator, atob/btoa,
	 * etc. on top of whatever setup_globals installed. */
	macsurf_js_setup_globals(heap->ctx);
	register_browser_globals(heap->ctx);
	global_heap = heap;
	*out_heap = heap;
	MS_LOG("js: heap created");
	return NSERROR_OK;
}

void js_destroyheap(struct jsheap *heap)
{
	if (heap == NULL) return;
	if (heap->ctx != NULL) duk_destroy_heap(heap->ctx);
	if (global_heap == heap) global_heap = NULL;
	free(heap);
}

/* fixes319c — pull the dom_document out of the html content and wire
 * it to the DOM bindings via macsurf_js_set_document. Without this,
 * macsurf_js_current_document stays NULL and every document.* call
 * (getElementById, querySelector, createElement) early-returns null,
 * which is why the t.html probe rows stayed at "(not run)" — the
 * console.* output landed in the log fine, but the DOM-write side
 * couldn't find #console-out / #nav-out / #b64-out.
 *
 * doc_priv comes from browser_window.c as hlcache_handle_get_content(c)
 * — i.e. struct content *. For HTML content this is actually an
 * html_content with a dom_document field inside. The public accessor
 * html_get_document() takes hlcache_handle*, not struct content*, so
 * we reach in directly via the private header. The accessor itself
 * just does `return ((html_content*)content)->document` so we use
 * the same pattern. */
#include "html/private.h"
extern void macsurf_js_set_document(dom_document *doc);

nserror js_newthread(struct jsheap *heap, void *win_priv, void *doc_priv,
		struct jsthread **out_thread)
{
	struct jsthread *thread;
	if (out_thread == NULL) return NSERROR_BAD_PARAMETER;
	*out_thread = NULL;
	if (heap == NULL || heap->ctx == NULL) return NSERROR_BAD_PARAMETER;
	thread = (struct jsthread *)calloc(1, sizeof(*thread));
	if (thread == NULL) return NSERROR_NOMEM;
	thread->heap = heap;
	thread->ctx = heap->ctx;
	thread->win_priv = win_priv;
	thread->doc_priv = doc_priv;
	*out_thread = thread;
	if (doc_priv != NULL) {
		html_content *htmlc = (html_content *)doc_priv;
		macsurf_js_set_document(htmlc->document);
		MS_LOG("js: thread document wired");
	}
	return NSERROR_OK;
}

nserror js_closethread(struct jsthread *thread)
{
	(void)thread;
	return NSERROR_OK;
}

void js_destroythread(struct jsthread *thread)
{
	free(thread);
}

/* NetSurf signature:
 *   bool js_exec(jsthread *, const uint8_t *, size_t, const char *name)
 * On CW8 PPC: bool == unsigned char, uint8_t == unsigned char,
 * size_t == unsigned long. Returns 1 on success (script ran cleanly),
 * 0 on parse/runtime error. Errors get logged via MS_LOG; the heap
 * stack stays clean either way. */
unsigned char js_exec(struct jsthread *thread,
		const unsigned char *txt, unsigned long txtlen,
		const char *name)
{
	int rc;
	(void)name;
	if (thread == NULL || thread->ctx == NULL || txt == NULL) return 0;
	if (txtlen == 0) return 1;
	duk_push_lstring(thread->ctx, (const char *)txt,
			(duk_size_t)txtlen);
	rc = duk_peval(thread->ctx);
	if (rc != 0) {
		MS_LOG(duk_safe_to_string(thread->ctx, -1));
		duk_pop(thread->ctx);
		return 0;
	}
	duk_pop(thread->ctx);
	return 1;
}

/* DOM event hooks. Body-stubbed for the bridge round — NetSurf core
 * doesn't call these on inline-script-only pages, so leaving them as
 * no-ops doesn't break anything that the bridge enables. Full event
 * dispatch wires up alongside the DOM-binding work in a separate
 * round. */

unsigned char js_fire_event(struct jsthread *thread, const char *type,
		struct dom_document *doc, struct dom_node *target)
{
	(void)thread; (void)type; (void)doc; (void)target;
	return 0;
}

void js_handle_new_element(struct jsthread *thread, struct dom_element *node)
{
	(void)thread; (void)node;
}

void js_event_cleanup(struct jsthread *thread, struct dom_event *evt)
{
	(void)thread; (void)evt;
}

/* macsurf_js_exec_script — internal alias kept for any caller that
 * still uses the pre-fix316 internal name. Forwards to js_exec. No
 * known call sites in tree but cheap to retain. */
bool macsurf_js_exec_script(struct jsthread *thread, const char *txt,
		size_t len)
{
	return (bool)js_exec(thread, (const unsigned char *)txt,
			(unsigned long)len, NULL);
}

#endif /* WITH_DUKTAPE */
