/*
 * MacSurf — browser_history_stub.c
 *
 * desktop/browser_history.c and desktop/textinput.c and desktop/frames.c
 * provide the real implementations of most history / caret / iframe
 * functions. The only history entrypoints with no upstream impl are
 * browser_window_history_add and browser_window_history_get_scroll
 * (only declared in browser_private.h).
 *
 * Licensed under GPL v2.
 */

#include <stddef.h>
#include "utils/errors.h"

struct browser_window;
struct hlcache_handle;
struct nsurl;
struct scrollbar;

nserror browser_window_history_add(struct browser_window *bw,
		struct hlcache_handle *content, struct nsurl *frag_id)
{
	(void)bw; (void)content; (void)frag_id;
	return NSERROR_OK;
}

nserror browser_window_history_get_scroll(struct browser_window *bw,
		float *sx, float *sy)
{
	(void)bw;
	if (sx) *sx = 0;
	if (sy) *sy = 0;
	return NSERROR_OK;
}

/* scrollbar_* — Mac OS 9 frontend uses Toolbox scroll-bar controls
 * directly, not desktop/scrollbar.c. Provide no-op stubs so any core
 * code that links against the API at all gets harmless behaviour. */
nserror scrollbar_create(int horizontal, int length, int full_size,
		int visible_size, void *pw,
		void (*cb)(void *pw, int msg, void *data),
		struct scrollbar **bar)
{
	(void)horizontal; (void)length; (void)full_size; (void)visible_size;
	(void)pw; (void)cb;
	if (bar) *bar = NULL;
	return NSERROR_OK;
}

void scrollbar_destroy(struct scrollbar *s) { (void)s; }

nserror scrollbar_set(struct scrollbar *s, int value, int bar_full)
{
	(void)s; (void)value; (void)bar_full;
	return NSERROR_OK;
}

int scrollbar_get_offset(struct scrollbar *s) { (void)s; return 0; }
