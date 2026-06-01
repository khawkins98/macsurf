# MacSurf 1.4 — Open House

**Released:** 2026-06-01
**Codename:** Open House
**Engine HEAD:** fixes352 (MacSurf side); macTLS unchanged from v1.3.1
**Verified on:** Power Macintosh G3 iMac, Mac OS 9.2.2

---

## The headline

**MacSurf 1.4 closes the JavaScript marathon.** Twenty-three GitHub issues went from open to closed in this release — `setTimeout` / `setInterval` / `requestAnimationFrame`, `window.location`, `window.history`, `URL` + `URLSearchParams`, `element.classList`, `element.style`, `Event` + `CustomEvent` + `MouseEvent` + `KeyboardEvent` constructors, `MutationObserver`, `DOMParser`, `FormData`, `localStorage`, `fetch`, `addEventListener` for `load` + `DOMContentLoaded`, plus `<details>` / `<summary>` toggle and the `hidden` attribute. A purpose-built probe page (`mactrove.com/t.html`) ran 19 JS probes on a G3 iMac and finished `JS 19/19 pass, 0 fail`.

Three diagnostic / power-user features land too: **`about:cache`**, **`about:memory`**, **`about:config`**, and **`about:perf`** now render real diagnostic pages with live counters. **View Source** is wired through `content_get_source_data` + a `data:text/html` URL so it renders the markup inline. **Find-in-page** opens a real Carbon dialog with a text input and Find / Cancel buttons, then highlights matches via NetSurf's textsearch.

Underneath that, the `data:` URL fetcher was a stub returning empty body since launch — fixed in this release. Every `data:` URL on every page now works, not just View Source.

![about:config rendered on a G3 iMac](https://raw.githubusercontent.com/mplsllc/macsurf/master/screenshots/macsurf-1.4-aboutconfig.jpg)

`about:config` showing the live nsoption table. Same Geneva chrome and orange-banded table styling as `about:cache` / `about:memory` / `about:perf`. Type any of the four into the URL bar; the new RFC 3986 scheme scanner in fixes351 stops the old `strstr("://")` heuristic from mangling them to `https://about:cache`.

---

## What's new

### JavaScript marathon

The JS bridge gained twenty-three closed issues in this release. The full list, with the fix-round that closed each one, is below. Each is exercised by the probe suite at `mactrove.com/t.html` (J0–J17 + J-write + J18 / J19 visual cards).

| Probe | Issue | Closed by |
|---|---|---|
| J1 | `setTimeout` / `setInterval` / `clearTimeout` / `clearInterval` | #103 fixes321 |
| J2 | `requestAnimationFrame` | #117 fixes322-324 |
| J3 | `window.location` (full surface: `href` + `protocol` + `host` + `hostname` + `port` + `pathname` + `search` + `hash` + `origin` + `assign` + `replace`) | #118 fixes323 + fixes350 |
| J4 | `window.history` (`back` + `forward` + `go` + `length` + `pushState` + `replaceState` + `state`) | #122 fixes324 + fixes350 |
| J5 | `URL` + `URLSearchParams` | #123 fixes325 |
| J6 | `element.classList.add / remove / toggle / contains` | #30 fixes326 + fixes349 |
| J7 | `element.style.<prop>` setters | #32 fixes327 + fixes349 |
| J8 | `load` + `DOMContentLoaded` via `window.addEventListener` | #31 fixes328 + fixes350 |
| J9 | `DOMParser` | #125 fixes331 |
| J10 | `FormData` | #124 fixes332 |
| J11 | `localStorage` (set / get / remove) | #46 fixes333 |
| J12 | `fetch` (shim presence) | #104 fixes334 |
| J13 | `Event` / `CustomEvent` / `MouseEvent` / `KeyboardEvent` constructors | #105 fixes339-340 |
| J14 | `MutationObserver` | #105 fixes339-340 |
| J15 | `element.matches` / `closest` / `getBoundingClientRect` / `contains` | fixes336-337 + fixes349 |
| J16 | `window.scrollTo` / `getComputedStyle` / `matchMedia` / `Promise` / `addEventListener` | fixes338 |
| J17 | `document.title` / `readyState` / `navigator.userAgent` | fixes341 |
| J18 | `<details>` / `<summary>` click-to-toggle | #110 fixes351 |
| J19 | `<span hidden>` attribute | #114 fixes329 |

Two structural bugs caught and fixed along the way:

- **fixes349 — IIFE per-element installer.** fixes342 had switched the JS init helper to `duk_peval_string_noresult` (which discards the eval result) but four per-element install sites in `macsurf_js_dom.c` still expected the function to be left on the stack for the subsequent `duk_dup(-2); duk_call(1)`. Result: `TypeError: [object Object] not callable` on first use of any element wrapper, abort mid-install, every element lost `classList` / `style` / `matches` / `closest` / `getBoundingClientRect` / `textContent` setter / `className` setter for the rest of its lifetime. Added a `macsurf_js__install_per_element` helper that does eval + `duk_pcall` with proper stack management; refactored the four sites.
- **fixes350 — window-level event dispatch.** `js_fire_event` only invoked the inline `window.on<type>` handler. Listeners registered via `window.addEventListener('load', fn)` (parked by fixes338 in `window._winListeners`) were never dispatched, so `load` and `DOMContentLoaded` were unreachable through the standard JS API. Now walks `_winListeners[type]` and pcalls each callable entry, then runs the inline `on<type>` handler as before.

### Diagnostic + power-user

- **`about:cache`, `about:memory`, `about:config`, `about:perf`** — all four now render. About:perf carries a live counters table fed by a new `macsurf__site_reformat_ms` global captured in `html_reformat`; about:config dumps the nsoption table; about:memory and about:cache surface the partition + memory + cache shape.
- **#99 root cause** was the URL-bar scheme heuristic. `strstr(r, "://")` only catches hierarchical schemes (http://, https://, file://, ftp://) — opaque schemes (about:, data:, javascript:, mailto:, resource:) use `<scheme>:<opaque>` with no `//`, so `about:cache` was forced to `https://about:cache`, nsurl parsed that as `host=about path=cache`, fetcher 404'd, page went blank. Replaced with a proper RFC 3986 scheme scan (`ALPHA *( ALPHA / DIGIT / "+" / "-" / "." )` terminated by `:`). Side effect: also unblocks `data:`, `javascript:`, `mailto:`, `file:`, `resource:` from URL-bar typing.

### View Source — real, not invented

The old path navigated to `view-source:<url>` which no fetcher in this codebase recognises (NetSurf core's `nsurl.h` enumerates only HTTP / HTTPS / FILE / FTP / MAILTO / DATA / OTHER). Navigation silently went nowhere.

The new path uses `content_get_source_data` to get the raw HTML bytes already in memory, HTML-escapes them into a `<pre>` block, percent-encodes the whole document into a `data:text/html;charset=utf-8,...` URL, and navigates to that. NetSurf's html handler renders it inline. Size cap at 32 KB for V1.

The fix sequence shows up clearly in the commit history: fixes352 builds the View Source helper, fixes352a rewrites the broken `data:` URL fetcher stub (it was returning `body=""` for everything — fixed here), fixes352b switches `text/plain` → `text/html` wrapped in `<pre>` because MacSurf has no inline text/plain handler.

### Find-in-page — real Carbon dialog

The previous "Find" was a `StandardAlert` reading "future round will install a Find dialog." This release installs the actual dialog.

The dialog is built programmatically with `CreateNewWindow` + `TENew` + a `ModalDialog`-style event loop, so no Rez source changes were needed. The kMovableModalWindowClass path was rejected by CarbonLib (`errInvalidWindowAttributesForClass`), so the dialog uses `kDocumentWindowClass` + `kWindowCloseBoxAttribute` — the same proven pattern as the main browser window. Last search term is cached for future Find Again wiring.

The dialog routes to `browser_window_search(bw, NULL, SEARCH_FLAG_FORWARDS, term)`, which highlights matches via the existing text-redraw path. **Important behavioural choice:** after the search returns, MacSurf explicitly resets scroll to `(0, 0)` instead of letting NetSurf core auto-scroll to the first match. The core's scroll-to-match math produces invalid coordinates on MacSurf (e.g., `scroll=(571, 335)` for a `content_width=949` page, stranding the user past the content edge); resetting to top keeps the highlight on the match and lets the user navigate normally.

### Other CSS / layout fixes that landed in 1.3.1 → 1.4

- **fixes318 — Repeating gradient + multi-stop fidelity.** Closes #145 #147 #148. The shorthand-gradient extractor now preserves `background-repeat`, handles 3+ color stops cleanly (intermediate stop becomes the visual "second distinct" colour for pinstripe recovery), and skips matches inside `repeating-linear-gradient(` so the inner `linear-gradient` substring isn't grabbed by accident.
- **fixes344b — Real alpha-aware gradient stops.** Full ARGB stops stored on an outer-struct side channel (`macsurf_gradient_full`) so RGBA + `transparent` no longer get truncated to opaque RGB.
- **fixes345 — Radial-gradient size + position prefix.** Parses `<W>px <H>px at <X>% <Y>%`, captures into the radial cascade, plotter uses it.
- **fixes346 — Pinstripe / repeating-pattern recovery.** When the source was `repeating-linear-gradient(white 0, white 1px, gray 1px, ...)` (mactrove's Platinum title-bar) the first == last detection now swaps last for the first distinct intermediate colour so the gradient becomes white→gray instead of white→white solid.
- **fixes347 — Pseudo-element background-image fetch.** The element-level fetch at the bottom of `box_construct_element` was firing for elements but never for pseudos, so `gen->background` stayed NULL forever. Now per-pseudo `html_fetch_object` runs from `box_construct_generate`. Pair with the `has_background` image-url trigger so the redraw path picks the bg-image as a background source.
- **fixes348 — Alpha-overlay gradient downgrade (closes #158).** Platinum pinstripe gradients like `linear-gradient(rgba(255,255,255,.5) 1px, transparent 1px)` were rendering as harsh black-to-white gradients because the painter ignored alpha. When a stop has alpha < 0xC0 OR the gradient has the transparent-pinstripe shape, the cascade now downgrades to NONE so the underlying `background-color` shows through. Fully-opaque gradients are untouched.

---

## How to upgrade

This is a **MacSurf source-side release**. macTLS is unchanged from v1.3.1.

- **Drop-in binary**: download **[MacSurf.sit](https://github.com/mplsllc/macsurf/releases/download/v1.4/MacSurf.sit)**, expand on Mac OS 9.1+ with CarbonLib 1.5+, launch.
- **From source**: pull the repo and rebuild in CodeWarrior 8. One file needs to be added to `MacSurf.mcp` for v1.4: `browser/netsurf/desktop/search.c` (provides `browser_window_search` for the new Find dialog). `content/textsearch.c` is already in.

---

## What's still open

- **#48 Bookmarks** — menu installs and Show works, but Add is backed by a session-only local array instead of NetSurf's `desktop/hotlist.c` API. The hotlist rewire is queued as a follow-on.
- **The long tail** (~60 open issues) — modern HTML5 / JS / CSS features the project tracks separately. Nothing blocks rendering of normal pages.
- **`<details>` cosmetic** — the toggle now works on click, but pages that use `display: none` for collapsible regions independently of `<details>` will still need their own click-to-toggle handlers (they always did; this isn't regressed, it just isn't a new framework feature).

---

## Bug reports + screenshots

Real-hardware screenshots and bug reports are exactly what this project still needs. If you've got a G3 or G4 sitting around, load 1.4 and see what breaks on the modern web. The full punch list is in [docs/status.md](../status.md).
