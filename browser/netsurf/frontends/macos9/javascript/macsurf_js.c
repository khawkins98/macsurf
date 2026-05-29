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

/* Pre-existing baseline console.log: appends space-separated args to a
 * 512-byte buffer and emits one MS_LOG line. Survived the fixes290
 * revert because it's a local static in this file, not part of the
 * Tier 1 console object that fixes288 added. */
static duk_ret_t native_console_log(duk_context *ctx)
{
	int n = duk_get_top(ctx);
	int i;
	char log_buf[512];
	size_t pos = 0;
	log_buf[0] = '\0';
	for (i = 0; i < n; i++) {
		const char *s = duk_safe_to_string(ctx, i);
		size_t slen = strlen(s);
		if (pos + slen + 2 < 512) {
			if (pos > 0) log_buf[pos++] = ' ';
			memcpy(log_buf + pos, s, slen);
			pos += slen;
		}
	}
	log_buf[pos] = '\0';
	MS_LOG(log_buf);
	return 0;
}

static void register_console_log(duk_context *ctx)
{
	duk_push_global_object(ctx);
	duk_push_object(ctx);
	duk_push_c_function(ctx, native_console_log, DUK_VARARGS);
	duk_put_prop_string(ctx, -2, "log");
	duk_put_prop_string(ctx, -2, "console");
	duk_pop(ctx);
}

/* ------------------------------------------------------------------ */
/* fixes316 — NetSurf-API surface.                                    */
/* These names are the actual symbols NetSurf core links against from */
/* desktop/browser_window.c, content/handlers/html/script.c, etc.     */
/* Pre-fix316 these resolved to no-ops in js_stub.c; with the !WITH_  */
/* DUKTAPE gate in that file, the symbols now belong to us.           */
/* ------------------------------------------------------------------ */

void js_initialise(void)
{
	MS_LOG("js: initialise");
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
	register_console_log(heap->ctx);
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
