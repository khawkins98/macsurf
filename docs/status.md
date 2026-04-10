# MacSurf Status

**Date:** 2026-04-10
**Milestone:** App builds, links, and runs on Mac OS 9

---

## 1. What Works Today

MacSurf builds with CodeWarrior 8 on Mac OS 9 and runs on a Power Mac G3/G4. The app:

- Launches successfully under Carbon on Mac OS 9.2
- Displays a menu bar (Apple, File, Edit, Go, Help)
- Opens a 640x480 document window titled "MacSurf" with scroll bars and a URL bar placeholder
- Runs a cooperative `WaitNextEvent` event loop with proper idle sleep
- Handles mouse, keyboard, update, activate, and high-level events
- Responds to File > Quit (Cmd-Q) to exit cleanly
- Passes all compile and link stages with zero errors

### Libraries in the Project

| Library | Source |
|---------|--------|
| `MSL_All_Carbon.Lib` | CodeWarrior MSL (C runtime, stdio, stdlib, string, math) |
| `CarbonLib` | Carbon API (windows, menus, controls, events) |
| `InterfaceLib` | Classic Toolbox compatibility (currently linked but may be removable) |

### Source Files (53 .c files in MacSurf.mcp)

**Frontend (18 files):**
`main.c`, `window.c`, `macos9_bitmap.c`, `macos9_fetch.c`, `macos9_download.c`, `macos9_utf8.c`, `clipboard.c`, `font.c`, `misc.c`, `plotters.c`, `schedule.c`, `corestrings_stub.c`, `fetch_stub.c`, `http_stub.c`, `browser_history_stub.c`, `misc_stub.c`, `js_stub.c`, `lwc_stub.c`

**Shims (5 files):**
`mac_file_io.c`, `mac_stat.c`, `mac_time.c`, `mac_dirent.c`, `mac_iconv.c`

**NetSurf Core (30 files):**
`browser.c`, `browser_window.c`, `gui_factory.c`, `netsurf.c`, `plot_style.c`, `content.c`, `content_factory.c`, `hlcache.c`, `llcache.c`, `fs_backing_store.c`, `no_backing_store.c`, `mimesniff.c`, `urldb.c`, `log.c`, `messages.c`, `nsoption.c`, `time.c`, `utf8.c`, `file.c`, `filepath.c`, `utils.c`, `hashtable.c`, `bloom.c`, `nsurl.c`, `parse.c`

---

## 2. What Is Stubbed Out

Every subsystem that isn't needed for the initial "show a window" milestone is stubbed with no-op implementations. These stubs link cleanly but do nothing.

### `corestrings_stub.c` — Interned String Table
- 136 `lwc_string *corestring_lwc_*` globals (all NULL)
- 181 `struct dom_string *corestring_dom_*` globals (all NULL)
- 5 `struct nsurl *corestring_nsurl_*` globals (all NULL)
- `corestrings_init()` / `corestrings_fini()` — no-ops

**To implement:** `corestrings_init` must call `lwc_intern_string` for every entry in `corestringlist.h`. This is required before any content can be processed — every MIME type check, HTML tag match, and URL scheme comparison uses these interned strings.

### `fetch_stub.c` — Network Fetch Subsystem
- `fetch_start` — returns NULL (no fetches)
- `fetch_can_fetch` — returns 0
- `fetch_http_code` — returns 0
- `fetch_abort`, `fetch_quit`, `fetcher_quit` — no-ops
- `fetch_multipart_data_*` — all no-ops

**To implement:** This is the critical path. `fetch_start` must open a TCP connection via Open Transport to the MacSurf proxy, send an HTTP request, and deliver the response back to the llcache via callbacks. All OT calls must be async (no blocking the event loop).

### `http_stub.c` — HTTP Header Parsing
- `http_parse_content_type`, `http_parse_cache_control`, `http_parse_content_disposition`, `http_parse_www_authenticate`, `http_parse_strict_transport_security` — all return NSERROR_NOT_FOUND
- All destroy functions — no-ops
- Cache-control/STS accessor functions — return 0

**To implement:** These parse HTTP response headers. Needed for content type detection. Can be implemented incrementally — `http_parse_content_type` is the most critical (determines how to render the response).

### `browser_history_stub.c` — Navigation History & Frames
- `browser_window_history_create/destroy/add/update/get_scroll` — no-ops
- `scrollbar_create/destroy/set/get_offset` — no-ops
- `browser_window_place_caret/remove_caret` — no-ops
- `browser_window_create/destroy/recalculate/invalidate_iframes` — no-ops
- `browser_window_create/recalculate_frameset` — no-ops
- `browser_window_handle_scrollbars` — no-op

**To implement:** History is needed for back/forward navigation. Scrollbars need to connect to the Carbon scroll bar controls in `window.c`. Frames/iframes are lower priority.

### `misc_stub.c` — Miscellaneous Subsystems
- Hotlist, global history — no-ops
- Image cache init/fini — no-ops
- Page info init/fini — no-ops
- System colours — no-ops
- Search web — no-ops
- Certificate chain handling — no-ops
- Download context — no-ops
- DOM namespace — no-ops
- Content handler inits (html, css, image, textplain) — no-ops
- `nscolour_update`, `idna_encode`, `save_pdf` — no-ops
- `html_get_id_offset` — returns 0

**To implement:** `image_cache_init` and content handler inits are needed for rendering. Most others can stay as stubs indefinitely.

### `js_stub.c` — JavaScript Engine
- All functions are no-ops (JS is disabled by design via `WITHOUT_DUKTAPE`)

**To implement:** Never. JavaScript is intentionally excluded to keep memory footprint low.

### `lwc_stub.c` — libwapcaplet (Interned Strings)
- `lwc_intern_string` — real implementation (malloc + copy)
- `lwc_string_destroy` — real implementation (free)
- `lwc__intern_caseless_string` — sets self-reference

**Status:** This is a working minimal implementation, not a stub. It correctly allocates lwc_string objects with the string data stored after the struct. It does not implement deduplication (every intern creates a new allocation) — this is acceptable for the initial build but will need optimization for memory efficiency.

---

## 3. Next Steps (Priority Order)

### Phase 1 — Display a Page from the Proxy

1. **Implement `corestrings_init`** — populate all interned string globals from `corestringlist.h`. Without this, no MIME type matching works.

2. **Implement `macos9_fetch.c` fetch backend** — connect to the MacSurf proxy via Open Transport. The fetch must:
   - Open an async TCP connection to the proxy (configurable host:port)
   - Send `GET http://target-url HTTP/1.0\r\nHost: target\r\n\r\n`
   - Receive the response and deliver it to the llcache callback
   - All OT calls must use async notifiers — never block WaitNextEvent

3. **Implement `http_parse_content_type`** — parse `Content-Type: text/html; charset=utf-8` from HTTP response headers. The hlcache uses this to decide which content handler to invoke.

4. **Implement plotters** — `plotters.c` must draw to the window's GrafPort. Start with `plot_rectangle` (for backgrounds) and `plot_text` (for text runs). Use QuickDraw `PaintRect` and `DrawString`/`TextFace`/`TextSize`.

5. **Implement font measurement** — `font.c` must measure text width and split positions. Use `TextWidth` and `CharWidth` from QuickDraw.

### Phase 2 — Usable Browser

6. **Wire up the URL bar** — let the user type a URL and navigate to it
7. **Implement scroll** — connect Carbon scroll bars to `browser_window_set_scroll`
8. **Implement history** — back/forward using the history stub
9. **Implement bitmap rendering** — `macos9_bitmap.c` must create offscreen GWorlds and blit to the window

### Phase 3 — Polish

10. **Proxy preferences** — let user configure proxy host:port in a dialog
11. **Error pages** — show connection errors in the window
12. **Memory optimization** — lwc deduplication, cache limits tuning
13. **Remove debug SysBeeps** — clean up diagnostic beeps from main.c

---

## 4. Architecture Notes

### Fetch Proxy Integration

```
┌─────────────┐     HTTP      ┌──────────────┐     HTTPS     ┌──────────┐
│  MacSurf    │ ──────────── │  MacSurf     │ ────────────── │  Web     │
│  Browser    │  Open        │  Proxy       │   TLS via Go   │  Server  │
│  (Mac OS 9) │  Transport   │  (VPS/LAN)   │                │          │
└─────────────┘              └──────────────┘                └──────────┘
```

The Mac sends a standard HTTP proxy request over plain TCP. The proxy (a single Go binary) fetches the actual page over HTTPS and returns the response as plain HTTP. This lets the Mac browse HTTPS sites without any TLS implementation on OS 9.

The proxy uses the standard HTTP proxy protocol — the Mac sends:
```
GET http://example.com/ HTTP/1.0
Host: example.com
```
The proxy fetches `https://example.com/`, strips the TLS, and returns the response.

### Event Loop / Scheduler Integration

```
main()
  └── while (!macos9_done)
        └── macos9_poll()
              ├── WaitNextEvent(sleep_ticks)
              ├── macos9_dispatch_event()
              └── macos9_schedule_run()
```

NetSurf's core uses `guit->misc->schedule(ms, callback, pw)` to request timed callbacks. The Mac frontend implements this via `schedule.c`:

- `macos9_schedule()` inserts callbacks into a sorted linked list keyed by TickCount
- `macos9_get_next_delay()` returns ticks until the next callback (used as WaitNextEvent sleep)
- `macos9_schedule_run()` fires all callbacks whose time has passed
- When fetches are active (`macos9_fetching = true`), sleep is forced to 1 tick for network responsiveness

This mirrors the RISC OS `riscos_poll()` pattern — the event loop is the scheduler.

### Key Constraints

- **No threads.** Mac OS 9 is cooperative multitasking. All work happens in the `WaitNextEvent` loop. Open Transport notifiers must queue work for the main loop, never call NetSurf core directly.
- **C89 only.** CodeWarrior 8 compiles in strict C89 mode. No `inline`, no `//` comments, no variadic macros, no designated initializers, no `for(int i...)`, no `restrict`.
- **No JavaScript.** `WITHOUT_DUKTAPE` is defined globally. All JS stubs return immediately.
- **No HTTPS.** The browser never makes TLS connections. All HTTPS is handled by the proxy.
- **64MB minimum RAM.** The target is Power Mac G3/G4 with 64MB+. The 8MB application heap partition should be sufficient for basic browsing.
- **Big-endian PPC.** All bitmap and network byte order operations must account for PowerPC big-endian architecture.

---

## 5. Known Issues

### Linker Warning
`MSL_All_Carbon.Lib` produces a warning about `uname` — MSL provides a stub that we also define in `utils/utsname.h`. This is harmless (our definition wins at link time).

### Memory Partition
The application requires an 8MB heap partition. The default CW8 partition is too small. Set via Get Info on the built application, or in the project's SIZE resource.

### SysBeep Debug Checkpoints
`main.c` currently contains SysBeep checkpoints (1 beep at entry, 2 after init, 3 before event loop, 4 at exit). These should be removed before release.

### Pre-Carbon Init Guard
Classic Toolbox init calls (`MaxApplZone`, `InitGraf`, etc.) are guarded with `#ifndef TARGET_API_MAC_CARBON`. Under Carbon (our target), these are skipped — Carbon initializes automatically.

### Header Shadowing
CW8 finds NetSurf's `utils/time.h` and `utils/string.h` when `#include <time.h>` or `#include <string.h>` is used. The prefix file cannot use these includes. Standard C string functions are declared directly in `utils/string.h` under `#ifdef __MWERKS__` to work around this.

### lwc_intern_string No Deduplication
The current `lwc_stub.c` implementation allocates a new lwc_string for every `lwc_intern_string` call, even for duplicate strings. This works but wastes memory. A hash table for deduplication should be added before heavy browsing use.

### corestrings Not Initialized
All 322 corestring globals are NULL. Until `corestrings_init` is implemented, no MIME type matching or HTML tag recognition works. This is the first thing that needs to be built for page loading.
