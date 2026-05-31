/*
 * MacSurf — macos9_js_click.c
 *
 * fixes320 (#116, #119 button-click path).
 *
 * Bridges MacSurf's macos9 mouse handler to the JS bridge's inline
 * onclick dispatch. NetSurf's html/interaction.c handles form gadget
 * activation and link navigation directly; it does NOT fire DOM
 * click events for inline `onclick="..."` handlers on plain buttons
 * or other elements.
 *
 * After browser_window_mouse_click(BROWSER_MOUSE_CLICK_1, x, y) fires
 * (in main.c), main calls macos9_dispatch_click_to_js(bw, x, y). We:
 *   1. Resolve bw->current_content to an html_content.
 *   2. Hit-test via box_at_point starting from htmlc->layout to find
 *      the deepest box containing the click coords.
 *   3. Walk up the box tree to find a node that has an `onclick`
 *      attribute. Clicks frequently land on text or icon boxes nested
 *      inside a <button>; the handler lives on the button itself.
 *   4. Hand the dom_element to macsurf_js_dispatch_event which reads
 *      the onclick attribute and pevals it in the JS context.
 *
 * Implementation lives in its own TU so it can include html/private.h
 * (needed to reach htmlc->layout) without dragging the contents into
 * unrelated frontend files.
 */

#include <string.h>
#include <stdbool.h>

#include "utils/ns_errors.h"
#include "netsurf/types.h"
#include "netsurf/browser_window.h"
#include "content/content.h"
#include "content/content_protected.h"
#include "content/hlcache.h"
#include "html/private.h"
#include "html/box.h"
#include "html/box_inspect.h"
#include <dom/dom.h>

#include "macos9.h"
#include "macsurf_debug.h"
#include "javascript/macsurf_js.h"

extern struct hlcache_handle *browser_window_get_content(
		struct browser_window *bw);
extern bool macsurf_js_dispatch_event(struct jscontext *ctx,
		struct dom_element *el, const char *event_type);

static bool macsurf_node_has_onclick(struct dom_node *n)
{
	dom_string *name = NULL;
	dom_string *val = NULL;
	dom_node_type type;
	bool has = false;

	if (n == NULL) return false;
	if (dom_node_get_node_type(n, &type) != DOM_NO_ERR) return false;
	if (type != DOM_ELEMENT_NODE) return false;

	if (dom_string_create((const unsigned char *)"onclick", 7, &name)
			!= DOM_NO_ERR || name == NULL) {
		return false;
	}
	if (dom_element_get_attribute((dom_element *)n, name, &val) == DOM_NO_ERR
			&& val != NULL) {
		if (dom_string_length(val) > 0) has = true;
		dom_string_unref(val);
	}
	dom_string_unref(name);
	return has;
}

bool macos9_dispatch_click_to_js(struct browser_window *bw, int x_ns, int y_ns)
{
	struct hlcache_handle *h;
	struct content *c;
	html_content *htmlc;
	struct box *box;
	struct box *target = NULL;
	int box_x = 0, box_y = 0;

	if (bw == NULL) return false;
	h = browser_window_get_content(bw);
	if (h == NULL) return false;
	/* content_get_type takes hlcache_handle*, NOT struct content*.
	 * Both are opaque pointer typedefs so C doesn't catch the
	 * mistake — symptom is a garbage vtable deref crash. */
	if (content_get_type(h) != CONTENT_HTML) return false;
	c = hlcache_handle_get_content(h);
	if (c == NULL) return false;
	htmlc = (html_content *)c;
	if (htmlc->layout == NULL) return false;
	if (htmlc->js_thread == NULL) return false;

	box = htmlc->layout;
	while (box != NULL) {
		struct box *next;
		next = box_at_point(&htmlc->unit_len_ctx, box, x_ns, y_ns,
				&box_x, &box_y);
		if (next == NULL) break;
		box = next;
	}

	/* fixes329 (#110) — <summary> click toggles parent <details>'s
	 * `open` attribute. Walk up from the clicked box to find a
	 * summary, then mutate its parent details. Skip the onclick
	 * dispatch for this case since the toggle is a native widget
	 * behaviour. */
	{
		struct box *cand = box;
		while (cand != NULL && cand->node != NULL) {
			dom_string *tname = NULL;
			dom_node_type ntype;
			if (dom_node_get_node_type(cand->node, &ntype) ==
					DOM_NO_ERR &&
			    ntype == DOM_ELEMENT_NODE &&
			    dom_node_get_node_name(cand->node, &tname) ==
					DOM_NO_ERR && tname != NULL) {
				bool is_summary = false;
				const char *n = dom_string_data(tname);
				size_t nl = dom_string_length(tname);
				if (nl == 7 &&
				    (n[0]=='S'||n[0]=='s') &&
				    (n[1]=='U'||n[1]=='u') &&
				    (n[2]=='M'||n[2]=='m') &&
				    (n[3]=='M'||n[3]=='m') &&
				    (n[4]=='A'||n[4]=='a') &&
				    (n[5]=='R'||n[5]=='r') &&
				    (n[6]=='Y'||n[6]=='y'))
					is_summary = true;
				dom_string_unref(tname);
				if (is_summary && cand->parent != NULL &&
				    cand->parent->node != NULL) {
					dom_string *open_name = NULL;
					if (dom_string_create(
						(const unsigned char *)"open", 4,
						&open_name) == DOM_NO_ERR &&
					    open_name != NULL) {
						bool has = false;
						dom_string *val = NULL;
						if (dom_element_get_attribute(
							(dom_element *)cand->parent->node,
							open_name, &val) ==
								DOM_NO_ERR &&
						    val != NULL) {
							has = true;
							dom_string_unref(val);
						}
						if (has) {
							dom_element_remove_attribute(
								(dom_element *)cand->parent->node,
								open_name);
						} else {
							dom_string *empty = NULL;
							if (dom_string_create(
								(const unsigned char *)"",
								0, &empty) ==
									DOM_NO_ERR) {
								dom_element_set_attribute(
									(dom_element *)cand->parent->node,
									open_name, empty);
								dom_string_unref(empty);
							}
						}
						dom_string_unref(open_name);
						{
							extern int browser_window_schedule_reformat(
								struct browser_window *bw);
							(void)browser_window_schedule_reformat(bw);
						}
						return true;
					}
				}
			}
			cand = cand->parent;
		}
	}

	target = box;
	while (target != NULL) {
		if (target->node != NULL &&
		    macsurf_node_has_onclick(target->node)) {
			break;
		}
		target = target->parent;
	}
	if (target == NULL || target->node == NULL) return false;

	{
		/* fixes320a — jsthread and jscontext have different struct
		 * layouts: jsthread->ctx is at offset 4 (after a heap*),
		 * jscontext->duk is at offset 0. Casting jsthread* to
		 * jscontext* makes ctx->duk read the wrong field — garbage,
		 * leading to a duk_err_setup_ljstate crash inside Duktape.
		 *
		 * Build a real jscontext on the stack with .duk = thread->ctx,
		 * pass &jsctx. dispatch_event only reads ->duk, so this is
		 * sufficient. */
		struct jsthread_layout {
			void *heap;
			duk_context *ctx;
			void *win_priv;
			void *doc_priv;
		};
		struct jsthread_layout *thr =
			(struct jsthread_layout *)htmlc->js_thread;
		struct jscontext jsctx;
		bool fired;
		jsctx.duk = thr->ctx;
		jsctx.win_priv = thr->win_priv;
		jsctx.doc_priv = thr->doc_priv;
		macsurf_debug_log_writef(
			"click: dispatch to dom_element=%p duk=%p",
			target->node, jsctx.duk);
		if (jsctx.duk == NULL) {
			MS_LOG("click: jsctx.duk is NULL, abort");
			return false;
		}
		fired = macsurf_js_dispatch_event(&jsctx,
			(struct dom_element *)target->node, "click");
		if (!fired) {
			MS_LOG("click: dispatch_event returned false");
		}
		return fired;
	}
}
