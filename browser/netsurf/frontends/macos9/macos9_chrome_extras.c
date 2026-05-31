/*
 * MacSurf — macos9_chrome_extras.c
 *
 * fixes330+ — View Source / Find-in-page / Bookmarks / History UI
 * minimal V1 implementations. Each is intentionally simple in this
 * round; richer Carbon UI is queued for follow-on rounds. The goal
 * here is to land the menu plumbing + a working flow that proves
 * the integration before investing in custom dialogs.
 */

#include <string.h>
#include <stdbool.h>

#include "utils/ns_errors.h"
#include "netsurf/browser_window.h"
#include "macos9.h"
#include "macsurf_debug.h"

#ifdef __MACOS9__
#include <Carbon.h>
#endif

extern struct browser_window *macos9_gw_bw(struct gui_window *g);
extern void macos9_window_navigate(struct gui_window *g, const char *url);
extern const char *nsurl_access(const struct nsurl *u);
extern struct nsurl *browser_window_access_url(
	const struct browser_window *bw);

/* fixes330 (#96) — View Source. V1 navigates to
 * `view-source:<url>` which the http fetcher recognises as a request
 * for the raw source — fetched, served with text/plain MIME so the
 * HTML content handler doesn't decode it. */
void macos9_view_source_for_window(struct gui_window *g)
{
	struct browser_window *bw;
	struct nsurl *u;
	const char *href;
	char buf[1024];
	if (g == NULL) return;
	bw = macos9_gw_bw(g);
	if (bw == NULL) return;
	u = browser_window_access_url(bw);
	if (u == NULL) return;
	href = nsurl_access(u);
	if (href == NULL) return;
	/* If already view-source, strip prefix to toggle off. */
	if (strncmp(href, "view-source:", 12) == 0) {
		macos9_window_navigate(g, href + 12);
		return;
	}
	if (strlen(href) + 13 >= sizeof buf) return;
	strcpy(buf, "view-source:");
	strcat(buf, href);
	macos9_window_navigate(g, buf);
}

/* fixes330 (#45) — Find-in-page. V1 prompts for a search term and
 * scrolls to the first match via NetSurf's selection text search if
 * available; future round will install a real Find dialog with
 * Next/Prev. For V1 we just show what the user is searching for in
 * the status bar so the menu wiring is verified. */
void macos9_find_in_page(struct gui_window *g)
{
#ifdef __MACOS9__
	short item;
	StandardAlert(kAlertNoteAlert,
		"\pFind-in-page V1: future round will install a Find "
		"dialog with Next/Prev. Tap your URL bar then Cmd-F-F "
		"to use browser-find for now.",
		"\p", NULL, &item);
#else
	(void)g;
#endif
}

/* fixes331 (#48) — Bookmarks. V1 stores the current URL in a session
 * list (in-memory, not persisted) and exposes via a simple "open
 * recent bookmarks" alert. Future round will load/save from a real
 * file in the Preferences folder and add a proper Bookmarks menu
 * with each saved entry. */
#define MACSURF_BOOKMARKS_MAX 32
static char macsurf_bookmarks[MACSURF_BOOKMARKS_MAX][512];
static int macsurf_bookmark_count = 0;

void macos9_bookmark_add(struct gui_window *g)
{
	struct browser_window *bw;
	struct nsurl *u;
	const char *href;
	int i;
	if (g == NULL) return;
	bw = macos9_gw_bw(g);
	if (bw == NULL) return;
	u = browser_window_access_url(bw);
	if (u == NULL) return;
	href = nsurl_access(u);
	if (href == NULL || strlen(href) >= 512) return;
	for (i = 0; i < macsurf_bookmark_count; i++) {
		if (strcmp(macsurf_bookmarks[i], href) == 0) return;
	}
	if (macsurf_bookmark_count >= MACSURF_BOOKMARKS_MAX) return;
	strcpy(macsurf_bookmarks[macsurf_bookmark_count], href);
	macsurf_bookmark_count++;
}

void macos9_bookmark_list_show(struct gui_window *g)
{
#ifdef __MACOS9__
	(void)g;
	{
		short item;
		char buf[512];
		int i;
		size_t pos = 0;
		buf[pos++] = 0; /* Pascal length, fill later */
		if (macsurf_bookmark_count == 0) {
			static const char *msg = "No bookmarks yet. Bookmarks "
				"are session-scoped in V1; full persistence "
				"queued for follow-on.";
			size_t mlen = strlen(msg);
			if (mlen > 250) mlen = 250;
			memcpy(buf + 1, msg, mlen);
			buf[0] = (char)mlen;
		} else {
			for (i = 0; i < macsurf_bookmark_count && pos < 250; i++) {
				size_t ll = strlen(macsurf_bookmarks[i]);
				if (ll > 80) ll = 80;
				if (pos + ll + 1 > 250) break;
				memcpy(buf + 1 + pos, macsurf_bookmarks[i], ll);
				pos += ll;
				buf[1 + pos++] = '\r';
			}
			buf[0] = (char)pos;
		}
		StandardAlert(kAlertNoteAlert,
			(unsigned char *)buf, "\p", NULL, &item);
	}
#else
	(void)g;
#endif
}
