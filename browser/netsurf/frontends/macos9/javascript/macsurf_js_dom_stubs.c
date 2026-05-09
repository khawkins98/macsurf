/*
 * MacSurf — macsurf_js_dom_stubs.c
 *
 * Historical: this file used to provide no-op out-of-line stubs for the
 * libdom public API that macsurf_js_dom.c calls, because libdom was not
 * fully linked into the build. With libdom now in MacSurf.mcp the real
 * implementations win and the local stubs just collide with them.
 *
 * Kept as an empty translation unit so the project file list does not
 * have to be re-edited; safe to remove from MacSurf.mcp at the next
 * project cleanup. No symbols are exported from here.
 *
 * Licensed under GPL v2.
 */

/* deliberately empty -- see header comment. Place a single unused
 * typedef so the file is not an empty translation unit (forbidden by
 * strict ISO C89). */
typedef int macsurf_js_dom_stubs_empty_marker_t;
