# MacSurf

A lightweight web browser for Mac OS 9 PowerPC, built on the NetSurf engine, paired with a simple TLS proxy.

## CLAUDE.md Maintenance

This file must be kept current. It falls out of date fast when not actively maintained, and stale context causes agents to repeat solved problems. Update as part of every round that changes project state — when a blocker is resolved, when a new subsystem lands, when the architecture shifts. The goal is that any new agent reading CLAUDE.md at the start of a session has an accurate picture of where the project actually stands. Detailed update protocol at the bottom of this file.

## Project Structure

```
macsurf/
├── browser/          # NetSurf fork with macos9 frontend
├── proxy/            # MacSurf Proxy (Go)
└── docs/             # Build and deployment docs
```

## Two Components

### 1. MacSurf Browser
A port of NetSurf to Classic Mac OS 9 using the Carbon API and CodeWarrior 8. Cross-compiled from Linux targeting PowerPC. Tabs disabled by default. Proxy config built into preferences.

### 2. MacSurf Proxy
A single Go binary that strips TLS — receives plain HTTP from the Mac, fetches via HTTPS, returns plain HTTP. No config files, no dependencies. Deployable on a VPS or local machine.

## Key Technical Constraints

- Development environment: Mac OS 9.1 on a Power Macintosh G3 Minitower (beige). All verified-working results come from this machine.
- Community compatibility target: Mac OS 9.2.2 on a Power Mac G4. Most-common active OS 9 setup today; not yet explicitly verified — open testing gap.
- Broader target range: Power Mac G3/G4, Mac OS 9.1-9.2.2, minimum 64MB RAM
- Compiler: CodeWarrior 8 (on-machine) or cross-compile GCC PPC from Linux
- No threading — OS 9 is cooperative multitasking, use WaitNextEvent loop
- No HTTPS in browser — all TLS handled by proxy
- JavaScript is handled in tiers, not banned:
  - **Base tier (G3 / 64MB floor):** Duktape 2.7.0 ES5 evaluator is linked into the base build and operational. Heavy / modern JS pages still go through the proxy render-and-flatten path.
  - **Enthusiast tier (G4 500+ / 256MB+):** same Duktape core as base tier, more ambitious inline-script scenarios enabled.
  - **Proxy tier:** headless Chromium/Playwright executes JS upstream and returns pre-rendered flat HTML. This is where real modern-site JS support lives.
- Carbon API for UI — works on OS 9 and early OS X

## Coding Conventions

- C for browser frontend (matches NetSurf codebase)
- Go for proxy
- Keep Mac Toolbox calls isolated in their own files (window.c, bitmap.c, font.c etc.)
- No external dependencies in proxy — stdlib only

## Carbon App Requirements

MacSurf is a Carbon CFM app running under CarbonLib on OS 9. For CarbonLib to fully engage, the binary MUST be identifiable as a Carbon fragment — otherwise `*InContext` calls crash at fixed addresses inside OTClientLib.

- **`'carb'` resource is mandatory.** Without it, CFM treats the binary as classic PEF, CarbonLib does not load as a dependency, and any `*InContext` OT call enters an uninitialized CarbonLib client context and crashes. This is the single most important requirement for a Carbon app on OS 9.
- **`MacSurf.rsrc`** contains the `'carb'` resource (zero-length, ID 0). It is a pre-compiled binary resource fork generated on Linux with a small Python script; CW8 links `.rsrc` files directly into the output resource fork with no Rez step. Must be listed in the CW8 project alongside the `.c` files.
- **`RegisterAppearanceClient()`** must be called at startup after `InitCursor()`, gated by a Gestalt check for Appearance Manager presence. Matches Classilla's `CBrowserApp` constructor pattern.
- **Skip** `InitGraf`/`InitFonts`/`InitWindows`/`InitMenus`/`TEInit`/`InitDialogs` under Carbon — Classilla explicitly skips them and so should MacSurf. Keep `InitCursor()` and `FlushEvents(everyEvent, 0)`.
- **No preemptive threads.** OS 9 is cooperative. Use `WaitNextEvent` for the UI event loop. OT yields happen through the notifier callback (see below).

## Open Transport Rules

MacSurf uses **plain (non-`InContext`) Open Transport calls**. This is verified against the Retro68 OT TCP demo and SSHeven, both of which run on real OS 9.2 hardware (external references — MacSurf itself develops and verifies on 9.1/G3).

- Use `InitOpenTransport()`, `OTOpenEndpoint()`, `CloseOpenTransport()` — **not** the `*InContext` variants. The InContext variants route through CarbonLib's OTClientLib, which has been the source of every crash we've seen.
- Use `OTUseSyncIdleEvents(ep, true)` plus a notifier that calls `YieldToAnyThread()` on `kOTSyncIdleEvent`. This is the cooperative-multitasking answer for synchronous OT calls — OT fires `kOTSyncIdleEvent` periodically while blocked, the notifier yields to the Thread Manager, and the app stays responsive without touching `WaitNextEvent` from inside the fetch.
- Use `OTInitDNSAddress(&dnsAddr, "host:port")` for address setup — one string, OT resolves hostname and port. Simpler than `OTInetStringToHost` + `OTInitInetAddress`.
- `OTBind(ep, NULL, NULL)` is legal and correct. No TBind ret buffer needed for outbound-only TCP clients.
- Include `<Threads.h>` — the classic Thread Manager is required for `YieldToAnyThread`.
- Reference implementations: [cy384/ssheven](https://github.com/cy384/ssheven) (production SSH client) and [cy384/miscellany retro68-demos/ot-tcp-demo.c](https://github.com/cy384/miscellany) (Apple `OTSimpleDownloadHTTP.c` adapted for Retro68). Both verified on OS 9.2.

## Prior Art

- **MacSurf appears to be the first serious NetSurf port to Classic Mac OS.** The netsurf-dev list has a single 2017 "Port to OS9?" thread with no follow-through. There is no prior NetSurf OS 9 port to reference.
- **Best networking references:**
  - [Classilla](https://sourceforge.net/projects/classilla/) — `macsockotpt.c` (NSPR's OT sockets layer) and `directory/c-sdk/ldap/libraries/macintosh/tcp-univhdrs/tcp.c` (standalone TCP over OT). Full Mozilla-era Carbon browser running on OS 9.
  - [cy384/ssheven](https://github.com/cy384/ssheven) — modern production SSH client, cooperative thread + OT.
  - [cy384/miscellany `retro68-demos/ot-tcp-demo.c`](https://github.com/cy384/miscellany) — shortest known-good OT HTTP client, ~220 lines.
- **Not references:** iCab (closed source), WaMCom (Classilla predecessor, same codebase), MoonlightOS (does not exist as far as we can find).

## Reference Frontends

NetSurf's RISC OS and AmigaOS frontends are the primary references for frontend architecture — both solved cooperative multitasking on non-POSIX systems. Study these before writing any frontend code.

- `frontends/riscos/` — closest analog to Mac OS 9
- `frontends/amiga/` — also cooperative multitasking

## Proxy Protocol

Standard HTTP proxy protocol — no custom protocol. The Mac sends a normal HTTP proxy request, the proxy fetches via HTTPS and returns plain HTTP. Compatible with any browser that supports HTTP proxies (Classilla works today as validation).

## Do Not

- Do not rely on in-app JS for heavy/modern sites — Duktape is ES5-only and intended for small inline scripts. Real-site JS support is still the proxy's job (render-and-flatten).
- Do not enable tabs by default
- Do not use preemptive threads anywhere in the browser
- Do not add external dependencies to the proxy stdlib core. The render-and-flatten subsystem is an optional separate service (can use Chromium/Playwright); the base HTTP-proxy binary stays stdlib-only.
- Do not target OS X only — Carbon must run on OS 9

## Build Environment

### Compiler
- CodeWarrior 8 Pro (with 8.3 update) running on Mac OS 9
- CW8 compiles in C89 mode — no C99, no C++ features
- CW8 defines `__MWERKS__` — use this to detect the compiler
- The project defines `__MACOS9__ 1` via the prefix file `macsurf_prefix.h`
- CW8 does NOT support: `inline`, `//` comments, variadic macros, forward enum declarations, C99 designated initializers, `for (int i...)`

### Prefix File
`browser/netsurf/frontends/macos9/macsurf_prefix.h` is injected before every compilation unit. It currently defines:
- `__MACOS9__ 1`
- `NO_IPV6 1`
- `TARGET_API_MAC_CARBON 1`
- `#include <MacTypes.h>` (first line — must stay first to prevent bool/true/false conflict)

`WITHOUT_DUKTAPE` is **no longer defined** — Duktape is linked into the base build. See [JavaScript Engine](#javascript-engine) below.

### Shims Layer
POSIX functionality is provided by stubs in `browser/netsurf/frontends/macos9/shims/`. These must be C89 compatible. Mac Toolbox headers must always be included before any bool/true/false definitions.

### Stub Headers
External dependencies not available on OS 9 are stubbed in `browser/netsurf/frontends/macos9/`:
- `libwapcaplet/libwapcaplet.h`
- `dom/dom.h`
- `libcss/libcss.h`
- `nsutils/endian.h`, `nsutils/time.h`, `nsutils/base64.h`, `nsutils/unistd.h`
- `sys/time.h`, `sys/types.h`
- `shims/iconv.h`, `shims/zlib.h`, `shims/stat.h`
- `css/utils.h`
- `parserutils/charset/utf8.h`

### Access Paths (CodeWarrior)
All non-recursive. User paths:
- `{Project}::patrick:macsurf-source Folder:`
- `{Project}::patrick:macsurf-source Folder:browser:netsurf:`
- `{Project}::patrick:macsurf-source Folder:browser:netsurf:frontends:`
- `{Project}::patrick:macsurf-source Folder:browser:netsurf:frontends:macos9:`
- `{Project}::patrick:macsurf-source Folder:browser:netsurf:frontends:macos9:shims:`
- `{Project}::patrick:macsurf-source Folder:browser:netsurf:frontends:macos9:parserutils:`
- `{Project}::patrick:macsurf-source Folder:browser:netsurf:frontends:macos9:parserutils:charset:`
- `{Project}::patrick:macsurf-source Folder:browser:netsurf:include:`
- `{Project}::patrick:macsurf-source Folder:browser:netsurf:content:`
- `{Project}::patrick:macsurf-source Folder:browser:netsurf:desktop:`
- `{Project}::patrick:macsurf-source Folder:browser:netsurf:utils:`

System paths:
- `{Compiler}:MacOS Support:Universal:Interfaces:CIncludes:`
- `{Compiler}:MacOS Support:MacHeaders:`
- `{Compiler}:MSL:MSL_C:MSL_Common:Include:`
- `{Compiler}:MSL:MSL_Extras:MSL_Common:Include:`
- `{Compiler}:MSL:MSL_C:MSL_MacOS:Include:`

### Linux Cross-Check
Use `gcc -fsyntax-only -std=c89 -pedantic -Dinline= -Ibrowser/netsurf/frontends/macos9/shims -Ibrowser/netsurf/frontends -Ibrowser/netsurf/include -Ibrowser/netsurf -include stdbool.h` to syntax-check frontend files on Linux before copying to Mac.

### Retro68 PPC Cross-Check
Stricter pre-flight that catches CW8-likely errors GCC's regular C89 mode misses (trailing commas in enums, comma-end of init list, K&R prototypes, etc.). Toolchain at `/home/patrick/Retro68/toolchain/bin/powerpc-apple-macos-gcc` (GCC 12.2.0).

```
$CC -fsyntax-only -std=c89 -pedantic-errors -Wall \
  -Wno-unused-parameter -Wno-unused-variable -Wno-long-long \
  -Dinline= -D__MACOS9__=1 -DTARGET_API_MAC_CARBON=1 -DWITH_DUKTAPE -DNO_IPV6 \
  -I browser/libwapcaplet/include \
  -I browser/libdom/include -I browser/libdom/include/dom -I browser/libdom/src \
  -I browser/libcss/include -I browser/libhubbub/include -I browser/libparserutils/include \
  -I browser/netsurf -I browser/netsurf/include \
  -I browser/netsurf/frontends/macos9 -I browser/netsurf/frontends/macos9/shims \
  -include stdbool.h <file.c>
```

`-Wno-long-long` is required because `frontends/macos9/shims/stdint.h` uses `long long` for `int64_t` (CW8 accepts as extension). **Do not** `-include macsurf_prefix.h` — its CW8-specific `time_t` / `struct tm` typedefs collide with Retro68's libc `time.h`. Filter results with `grep -E "browser/.*error:"` to ignore Retro68 sysroot noise (e.g. `sys/stat.h` `mode_t` undefined, unrelated to MacSurf code). Green Retro68 ≠ green CW8 — CW8 is the source of truth — but red Retro68 is always real.

### Project File List (470 .c files)
Added to MacSurf.mcp:
- 12 frontend `.c` files
- 5 shim `.c` files
- 10 NetSurf core `.c` files (utils + content + desktop)
- 15 libparserutils
- 30 libhubbub
- 95 libdom
- 303 libcss

`MacSurf.rsrc` (pre-compiled binary `'carb'` resource, generated on Linux) must also be in the project — CW8 links `.rsrc` files directly into the output resource fork with no Rez step. The `*_stub.c` files exist on disk but are NOT in the project file list. See [docs/research/architecture-inventory.md](docs/research/architecture-inventory.md) for the full breakdown.

### Adding new .c files
When a change introduces a new `.c` file, mention it plainly so the user can add it to the project. **Do NOT edit `MacSurf.mcp` and do NOT include it in fix zips** — the user maintains the project file list on the Mac side through the CW8 IDE, and a Linux-edited `.mcp` will clobber their local changes. Just list the new filename(s) in the handoff and let the user add them.

### Library Dependency Chain — COMPLETE

All five NetSurf core libraries are ported and C89-clean:

| Library | .c files | Status |
|---|---:|---|
| libwapcaplet | (via lwc_stub.c) | ✓ done at v0.1 |
| libparserutils | 15 | ✓ commit 8074a74 |
| libhubbub (HTML5 parser) | 30 | ✓ commit fd8d915 |
| libdom (DOM implementation) | 95 | ✓ commit 744232d |
| libcss (CSS parser + cascade) | 303 | ✓ commit 02628cf |
| **Total in MacSurf.mcp** | **443** | |

Combined LOC: ~125K. Stub footprint replaced: 3,688 lines (parserutils utf8.h + dom.h + libcss.h). All four port audits + execution reports live in [docs/research/](docs/research/):
- [parserutils-port.md](docs/research/parserutils-port.md)
- [libhubbub-port.md](docs/research/libhubbub-port.md)
- [libdom-port.md](docs/research/libdom-port.md)
- [libcss-port.md](docs/research/libcss-port.md)

All five libraries are linked and the NetSurf core wiring (HTTP fetcher, content factory, CSS/HTML handlers, end-to-end render) landed across the v0.2 → v0.3 transition. The wiring audit is preserved at [docs/research/netsurf-core-wiring.md](docs/research/netsurf-core-wiring.md) for historical reference.

### Library port audit checklist

When auditing a new C99 library for CW8 / strict C89, grep for:
- `inline` keyword
- `//` line comments (start-of-line AND trailing — but EXCLUDE URLs in `/* */` block comments, especially `http://www.opensource.org/licenses/...`)
- C99 designated initializers (`^\s*\.\w+\s*=`) — and **count instances per file**, not just file count. format_list_style.c had 47 in one file.
- For-scope declarations: integer types AND **pointer-type variants** (`for (TYPE *NAME = ...)`, `for (const TYPE *NAME = ...)`). The libcss audit missed pointer-type for-scope and undercounted by 10 sites.
- `restrict` keyword
- Compound literals
- `__VA_ARGS__` variadic macros
- `long long`
- Variable-length arrays
- Flexible array members
- Forward enum declarations
- `__attribute__` / `__builtin_*`
- `snprintf` / `vsnprintf`
- `%zu` / `%zd` printf formats
- `<iconv.h>`, `<errno.h>`, `<strings.h>`, `<sys/types.h>` and other POSIX
- **GNU union casts** — `(union_type)0` or `(typedef_name)expr` where the typedef resolves to a union. Standard C89 forbids casting to union types. The libcss audit missed 5 sites of `(css_fixed_or_calc)0`.
- **Union initializers using designated syntax** — `{.field = value}` for a typedef'd union looks identical to a struct designated init in grep output. C89 union initializers must use `{value}` (positional, first member only).
- Build-time codegen (`gperf`, perl scripts, `.inc` files included from `.c` files)
- Existing MacSurf stubs in `frontends/macos9/<libname>/` that will conflict with the real headers

## JavaScript Engine

- Duktape 2.7.0 is fully linked and operational in the base build.
- ES5 evaluator confirmed working — stress tests pass including closures, prototypes, regex, JSON, promises, recursion, matrix multiply, Mandelbrot.
- `js_newheap` / `js_destroyheap` / `js_exec` lifecycle working.
- `WITHOUT_DUKTAPE` has been removed from `macsurf_prefix.h`.
- Duktape source files live in [browser/libduktape/](browser/libduktape/).
- `duk_config.h` is hand-crafted for Mac OS 9 PPC CW8: `DUK_USE_BYTEORDER=3`, `DUK_USE_PACKED_TVAL`, `DUK_USE_ALIGN_BY=8`, `DUK_USE_NATIVE_CALL_RECLIMIT=128`.
- JS glue files live in [browser/netsurf/frontends/macos9/javascript/](browser/netsurf/frontends/macos9/javascript/).

## Browser Chrome

- Pixel-based scrolling operates on `content_get_height()`, not the v0.1 text-line model.
- Address bar routes through `browser_window_navigate` with `nsurl_create` URL normalization (prepends `http://` if no scheme).
- Back, Forward, Reload, Home buttons wired to real NetSurf navigation APIs.
- Status bar displays NetSurf status messages and hovered link URLs.
- Title bar auto-updates via `gui_window_set_title` from page content.
- Window resize triggers `browser_window_schedule_reformat` via a deferred flag pattern to prevent re-entrant layout.
- `MACSURF_HOME_URL` defined in `macsurf_config.h`.
- v0.1 fallback path (`strip_html` + direct `DrawText`) has been removed. Full NetSurf pipeline is the only rendering path.

## Mouse Wheel / Input Devices

- **No Carbon wheel handler.** `kEventMouseWheelMoved` is **not available in CarbonLib** on OS 9 — Apple's own `CarbonEvents.h` marks the event class as `Mac OS X: 10.0+ in Carbon.framework; CarbonLib: not available`. fixes134 attempted to install a handler and the Mac crashed with illegal-instruction at `19DBDEB8` because CarbonLib's dispatcher destabilizes when asked about events whose class was never back-ported. Root-caused and disabled in fixes140. See [browser/netsurf/frontends/macos9/macos9_wheel.c](browser/netsurf/frontends/macos9/macos9_wheel.c) — `macos9_wheel_install()` is retained as a visible no-op for ABI stability (Mac-side main.c may still reference it).
- **fixes141 — interim defensive hardening.** Even with the Carbon wheel handler disabled, spinning the wheel under MacSurf still dropped into MacsBug with `Undefined A-Trap at 1BDC54E0` (no procedure name — execution in garbage memory). fixes141 narrowed the `WaitNextEvent` mask to an explicit whitelist of classic event kinds and added a matching whitelist guard at the top of `macos9_dispatch_event` so any unknown event class is silently dropped before touching any Toolbox or browser-core code. This is hardening, not diagnosis — the underlying crash is likely inside CarbonLib or USB Overdrive's trap patches and cannot be debugged further without capturing a real MacsBug stack, which requires an ADB keyboard the user does not currently have. **Proper wheel-crash diagnosis deferred until ADB hardware is available.**
- **USB Overdrive — current recommendation: "Do Nothing" on the Scroll Wheel action** until the wheel-crash root cause is understood. Users should configure USB Overdrive's Scroll Wheel setting to "Do Nothing" (or not install a wheel binding at all) when MacSurf is frontmost. Scrolling works via scroll bar, keyboard arrows, Page Up/Down, and Home/End. The previous recommendation (Up/Down arrow keys) is valid in principle — it flows through `macos9_handle_key_down` — but may still trigger the underlying crash if USB Overdrive's trap patches touch state before the synthesized key event reaches us. See [docs/usb-overdrive.md](docs/usb-overdrive.md).
- **Complete scroll-input set on OS 9 without the wheel:** scroll-bar drag, keyboard arrows, Page Up/Down, Home/End. All keyboard-sourced paths are tested and working. Carbon-native wheel events are architecturally out of reach on this platform regardless of the fixes141 defensive work.

## Rendering Pipeline (v0.3 — native CSS)

- HTTP fetcher registered for `http:` and `https:` schemes via OT proxy at `116.202.231.103:8765`.
- Resource fetcher serves real CSS content for `resource:default.css`, `resource:internal.css`, `resource:quirks.css` (`macos9_fetcher_stubs.c`).
- `no_backing_store.c` returns `NSERROR_NOT_IMPLEMENTED` from store and fetch.
- Event-loop sleep shortens to 1 tick while any fetcher is active (`macos9_fetching || macos9_stub_fetcher_active() || macos9_http_fetcher_active()`) so NetSurf's fetcher ring progresses via `fetch_send_callback` continuations every pass. There is **no** explicit `fetch_poll()` call.
- **Full NetSurf pipeline executes: fetch → parse → CSS cascade with native var() resolution → layout → plot.**
- **Real HTML rendering with styled text, colours, fonts, layout all working natively.** MacTrove (Drupal 11 site) loads with body background, card chrome, link colours, and theme fonts resolving correctly from CSS custom properties. Verified signal: title bar shows `cp res OK`.
- **Architectural foundation:** [docs/research/state-survey-2026-04-18.md](docs/research/state-survey-2026-04-18.md) and [state-survey-2026-04-19.md](docs/research/state-survey-2026-04-19.md). The 2026-04-19 survey in particular (§A7) explicitly scoped three paths for var() support — native libcss, proxy preprocessor, browser preprocessor — and chose native. Without that scoping, the fast-looking proxy shortcut would have blocked the real fix. Treat both surveys as load-bearing architectural refs for any future CSS-layer work.
- Screenshot canonical location: `screenshots/v0.3-mactrove-fixes139.png` (user-saved from the 2026-04-20 session).
- Carbon partition bumped to 16 MB preferred / 8 MB minimum to accommodate libcss allocation footprint on real pages (CSS_NOMEM blocker long resolved; see Gotchas).

### Known feature gaps (not pipeline bugs)

- **`gap` / `row-gap`** — parsed at single-value fidelity only; `row-gap` shares storage with `column-gap`, so two-value `gap: A B` loses the first value. See the row-gap entry in Known Gotchas for the bit-packing path required for full-fidelity split.
- **Flex `justify-content` / `align-content` / `order`** computed by libcss but unread by `layout_flex.c`.
- **`border-radius`, `box-shadow`, gradients, transforms** — not parsed; cosmetic loss only.
- **Image content handlers** not linked — `<img>` renders as a placeholder box.
- **URL field input fails on the initial window, works on File → New Window** — content redraw during `browser_window_create` overdraws the URL rect visually while TextEdit is still functional internally (per 2026-04-18 survey §1).
- **Mouse wheel still crashes** on real G3/G4 hardware after `kEventMouseWheelMoved` was disabled in fixes140 and `WaitNextEvent` mask + dispatch whitelist hardened in fixes141. Linux-source hypothesis space exhausted; further progress requires direct MacsBug `wh`/`sc`/`ip`/`dm sp` capture, which needs an ADB keyboard the user does not currently have. **Do not redo the Linux-side audit** — it's documented at length in the git history (fixes140-147 commit messages).

## Native CSS Custom Properties

Shipped incrementally across fixes133-139. Native libcss implementation, not a proxy preprocessor. Per-document scope, tokens preserved through cascade, resolved at selection time (option c from the 2026-04-19 architecture decision).

- **Scope.** Custom property definitions captured from any rule with `--name: value` syntax (simplified from the full CSS spec which restricts to `:root`, `html`, `*` — treated as globally scoped within document for this implementation, which matches observed behaviour of every real stylesheet we've looked at).
- **Capture.** Each stylesheet keeps a `custom_properties` linked list of `css_cp_entry { lwc_string *name; css_cp_token *tokens; uint32_t n_tokens; next; }`. Populated from `handleDeclaration` when the first token is an `IDENT` whose idata starts with `--`. See [browser/libcss/src/parse/language.c](browser/libcss/src/parse/language.c) and [browser/libcss/src/parse/custom_properties.c](browser/libcss/src/parse/custom_properties.c).
- **Resolution.** `var(--name)` is resolved at cascade time via token substitution before the property-specific parsers run. The select-ctx-wide aggregate table combines all sheets' tables with last-write-wins over source order, matching author cascade. Nested `var()` is resolved recursively with a depth-10 cap to prevent infinite recursion on circular references.
- **Fallbacks.** `var(--name, fallback)` supported. Fallback can itself contain `var()`.
- **!important.** Preserved through substitution.
- **Keystone fix (fixes139).** `lex.c` `CDCOrIdentOrFunctionOrNPD` — when `--` is followed by `startNMStart(c)`, append and continue into `IdentOrFunction` rather than emitting `CHAR('-') + IDENT('-foo')`. Without this, libcss's CSS 2.1-era lexer splits `--foo` into two tokens that the declaration parser rejects. fixes133's capture logic was sound; it never had a chance to run until fixes139 landed the lexer branch. See [browser/libcss/src/lex/lex.c](browser/libcss/src/lex/lex.c).

## Native CSS3 Strategy

MacSurf handles modern CSS natively in libcss and the layout engine rather than preprocessing via the proxy. This preserves the "real web browser running natively on Mac OS 9" value proposition. The proxy strips TLS and optionally renders-and-flattens JavaScript-heavy sites; it does **not** preprocess CSS for browser limitations.

Native support landed:

- CSS custom properties (`var()`) — full pipeline including the lexer keystone fix (see Known Gotchas).
- `gap` / `row-gap` — single-value fidelity (column-gap storage shared); see "Known feature gaps" for the deferred two-value split.

Features that remain unsupported and degrade gracefully to block layout or flat rendering: CSS Grid (collapses to block), `box-shadow`, `transform`, `transition`, `animation`, gradients, `clip-path`, `mask`. These are cosmetic in most cases and their absence does not prevent page comprehension.

## Build State

- **Branch state (2026-05-09).** Active branch is `recovery`. `master` and `moonshot` lag behind v0.3 work; `revive-fixes-no-regression` carries fixes through 328; `v0.3-rendering` carries fixes through 318. The recovery branch is where post-regression rebuilding happens — last shipped zip is **fixes333** (libdom internal-include path fix + `fallthrough`/`FLEX_ARRAY_LEN_DECL` macros + trailing-comma in `html_script_element.h`). Hardware verification of the post-fixes169 history is incomplete; check the git log on each branch before quoting state.
- **Fix-zip numbering is monotonic** per user convention — the next round after the last shipped zip on disk under `Old Zips/`. **First ship of a session: ASK** before committing to a number; memory has been wrong about this in past sessions.
- MacSurf v0.3 has rendered real live web pages on G3 hardware with native CSS custom property support. First confirmed page was MacTrove (`http://mac.mp.ls/`), 2026-04-19, via the full NetSurf pipeline. v0.2 baseline (plain text, JS, OT networking) remains stable. Whether the current `recovery` HEAD reproduces v0.3 rendering on hardware is **unverified** — every fix round lists Retro68 syntax-clean as the strongest Linux signal; CW8 build outcome is the user's authoritative call.
- CW8 project file is [browser/netsurf/frontends/macos9/MacSurf.mcp](browser/netsurf/frontends/macos9/MacSurf.mcp). The agent does NOT edit it; new `.c` files are mentioned plainly in handoff so the user adds them on the Mac.
- Carbon partition: **16 MB preferred / 8 MB minimum** (`MWProject_PPC_size` / `MWProject_PPC_minsize`). Anything smaller starves libcss and triggers CSS_NOMEM mid-cascade on real pages.
- Flat-folder build approach — all `.c` files in one folder, one search path.
- Remove Object Code is required before every rebuild after file changes.
- MacsBug is installed on the G4 for pipeline debugging; `MS_LOG` checkpoints + the file-backed diagnostic log on Desktop (`MacSurf Debug.log`) are the post-crash forensics channels.

For the per-fix history (what each round did, which crashes are still open, which features are pending), read `git log --oneline` on the relevant branch — the durable architectural facts live in this file's other sections, not in a fix-by-fix narrative here.

## Regression Audit Checklist

A single fix round (fixes149-152) shipped log infrastructure with the init never wired into `main()`, silently discarding three rounds of instrumentation. This checklist is the durable response — any new subsystem shipped as part of a fix round MUST, before the round is closed, satisfy all three:

1. **Init call wired.** Grep the entrypoint (`main.c main()` for MacSurf) for the init function name. `grep -c macsurf_foo_init main.c` must return a non-zero count. If zero, the subsystem is linked but dead.
2. **Smoke test confirming it runs.** Either a SheepShaver smoke launch (boot, relaunch, confirm the subsystem's externally-visible artefact — file, title-bar message, menu item — is present) or a hardware cycle. "Linux syntax check passes" is NOT a smoke test; syntax passes on code that is never executed.
3. **Dependency documented.** Add a one-line entry under this section's table for new infrastructure:

| Subsystem | Init function | Init call site | Externally-visible artefact at startup |
|---|---|---|---|
| File-backed diagnostic log | `macsurf_debug_log_init` | `main.c main()` after `FlushEvents` | `MacSurf Debug.log` on Desktop with `=== MacSurf startup ===` entry |
| Open Transport | `InitOpenTransportInContext` | `main.c main()` after log init | `ot_initialized = true` (internal) |
| NetSurf core | `netsurf_init` | `main.c main()` after OT init | Window shows with content pipeline live |
| Carbon Appearance | `RegisterAppearanceClient` | `main.c main()` after `InitCursor` | Controls render with platinum theme, not classic |

**When reviewing / shipping a fix round:** if the round adds or touches a subsystem in the table above, verify the init path still fires. If the round adds a new subsystem, add an entry. Missed-init is the exact class of regression this table catches — `macsurf_debug_log_write` short-circuits silently when `g_log_open == 0`, so a missed init produces zero stderr, zero toolbox output, and a "successful" run that wrote nothing to disk.

## File-Backed Diagnostic Channel

Shipped in fixes149. Writes one CR-terminated line per log call to
**`MacSurf Debug.log`** on the Desktop, flushing after every write
(`FlushVol` + `SetFPos` pair) so the file survives illegal-instruction
crashes, frozen Macs, and forced restarts. This is the **primary
post-crash back trace channel for MacSurf** on hardware we can't
attach MacsBug to.

- API: [browser/netsurf/frontends/macos9/macsurf_debug_log.h](browser/netsurf/frontends/macos9/macsurf_debug_log.h). `macsurf_debug_log_init()` at startup, `_close()` at shutdown, `_write(str)` for literal strings, `_writef(fmt, ...)` for minimal printf (`%d`, `%ld`, `%p`, `%s`, `%%`).
- `MS_LOG(msg)` now dual-channels: title bar (live feedback) **and** log file (durable record). File write comes first so a SetWTitle-adjacent crash still leaves the log entry on disk.
- `_writef` uses a hand-rolled formatter, NOT MSL's `vsnprintf` (unreliable on CW8 Carbon MSL). Supports only the format specifiers used by MacSurf instrumentation. Output is hard-capped at 255 bytes.
- Log file location: Desktop (via `FindFolder(kOnSystemDisk, kDesktopFolderType, ...)`). If FindFolder fails the log is silently inert — init does not crash, subsequent calls no-op.
- Reading the log: open `MacSurf Debug.log` in SimpleText. Each line is one log call. Crash forensics = "the last N lines before the log ends show the code path that was executing when the Mac died."
- **Release builds (`MACSURF_RELEASE` set) compile the channel to empty stubs** — symbols stay exported for link compatibility, but no file operations happen.
- **Gotcha:** the channel depends on HFS actually committing writes. `FlushVol` forces this, but if a volume is full / dismounted / read-only the write silently fails. If the log file exists but is truncated or stale after a crash, the HFS journal didn't catch up — retry the crash with a different volume or add an extra tick of delay after each write.
- Don't replace existing `MS_LOG` call sites with `macsurf_debug_log_writef` unless you need format arguments. `MS_LOG(literal)` is ergonomically equivalent and keeps the title bar updated for free.

## Docs

- [docs/macsurf-architecture.md](docs/macsurf-architecture.md) — Full platform architecture: rendering modes, proxy services, template system, milestone plan
- [docs/research/architecture-inventory.md](docs/research/architecture-inventory.md) — Snapshot of what currently exists in the repo and on the proxy host (no decisions, just facts)
- [docs/research/window-architecture-2026-04-22.md](docs/research/window-architecture-2026-04-22.md) — Window-framework architecture research (originally fixes161). State/event/redraw/scroll inventory of the Mac OS 9 frontend, architectural problem list, proposed unified window-state model, and a 6-round refactor plan. The refactor partially landed (fixes162–166) but the branch then went through a regression cycle — verify against current code before quoting the plan as still in force.
- [docs/status.md](docs/status.md) — Project status, milestones, test environment
- [docs/codewarrior-setup.md](docs/codewarrior-setup.md) — How to install CodeWarrior 8 and build on a real Power Mac
- [docs/deploying-proxy.md](docs/deploying-proxy.md) — How to deploy the Go proxy

## SheepShaver as a Testing Tool

MacSurf is built on Linux but target-tested on real OS 9 hardware. SheepShaver (an OS 9 emulator) is a useful *partial* substitute — **not a full one**.

- **SheepShaver setup lives at** `/home/patrick/Webs/MAC/sheepshaver/` — shared folder at `shared/`, prefs at `prefs`, Xvfb on `:99`. Shared folder uses `.finf/` (32-byte FInfo per file) + `.rsrc/` (raw resource fork) sidecars for Mac metadata.
- **Run the SheepShaver AppImage** from `/tmp/squashfs-root/AppRun` with `DISPLAY=:99 APPIMAGE=/tmp/squashfs-root HOME=/home/patrick`.
- **Decode a .hqx build into the shared folder** with `/tmp/binhex_decode.py <hqx> <shared-folder>`. Writes the file + `.finf/<name>` + `.rsrc/<name>` with correct APPL/MPLS type/creator and the full cfrg/carb/SIZE resource fork.
- **Hand-built resource forks don't work.** Shipping just the PEF data fork (the `/home/patrick/Webs/macsurf/MacSurf` checkin) leaves out `cfrg` (Code Fragment resource) and OS 9 refuses to launch it. The CW8 build-on-Mac workflow produces the full resource fork at link time; replicating it on Linux requires either the .hqx or reconstructing cfrg by hand (the hand-built variant was attempted and OS 9 still rejected the launch).

**What SheepShaver IS useful for:**
- Smoke test — does the build launch at all, does Carbon init succeed, do OT/CarbonLib dependencies resolve
- Rendering regression checks — does MacTrove render, does var() still resolve, does layout not regress
- Non-hardware-specific logic bugs — compile errors that make it through Linux syntax check but not the real linker, obvious toolbox misuse

**What SheepShaver is NOT useful for:**
- Hardware-specific crashes (wheel crash, scroll-bar click crash). SheepShaver's CarbonLib + Control Manager emulation is more forgiving than real hardware. A green light in SheepShaver does NOT mean the G3/G4 will also be green.
- USB Overdrive interactions — doesn't exist in the emulator
- Real network behavior — `/home/patrick/.sheepshaver_prefs` as shipped has no usable ethernet config, so the initial fetch to the proxy at `116.202.231.103:8765` blocks until timeout (~2 min) without yielding. This is a test-env artifact, not a MacSurf bug.
- Timing-sensitive behavior — JIT / coop-scheduler pacing differs from real PPC

**Workflow:** use SheepShaver opportunistically — launch a new build, confirm it boots, click around. If anything hardware-specific is the suspect, move to real G3/G4. Don't treat SheepShaver "passed" as a substitute for hardware-side verification on wheel/scroll/USB-driven bugs.

## Known Gotchas

- **`kInitOTForApplicationMask` and `kOTInvalidConfigurationRef` are not defined in CW8's OT headers.** Either `#define` them manually (`kInitOTForApplicationMask = 0x00000002`) or avoid them entirely by using the plain `InitOpenTransport()` path.
- **Including `<OpenTransport.h>` is safe** now that `kWindowStandardHandlerAttribute` has been removed from `CreateNewWindow`. An earlier crash that seemed like it was caused by including the header was actually the window-attribute bug manifesting later.
- **No `'carb'` resource → OTClientLib crash at a fixed address.** If the same instruction crashes every time somewhere inside OTClientLib, the cause is almost always that the binary is not a recognized Carbon fragment. Add `'carb'` before debugging anything else.
- **CW8 C89:** no `inline`, no `//` comments, no variadic macros, no forward enum declarations, no C99 designated initializers, no `for (int i...)`. All variables at the top of their enclosing block.
- **Carbon partition must be at least 16 MB preferred.** Set in CW8 under "PPC PEF" → Application Heap Size / Minimum Heap Size (`MWProject_PPC_size` / `MWProject_PPC_minsize` in the `.mcp` XML). A 4 MB partition (the CW8 default) runs out mid-CSS-cascade on a moderately-sized real page — `css_select_style` returns `CSS_NOMEM` somewhere around element 380. libcss allocates via raw `malloc`/`calloc` with no NetSurf wrapper, so OOM in libcss really does mean OS-heap exhaustion. Classilla's default is 32 MB; 16 MB is MacSurf's floor. See [docs/research/state-survey-2026-04-18.md](docs/research/state-survey-2026-04-18.md) §2.
- **CW8 PPC miscompiles `long long` / `int64_t` multiply-by-constant.** `(long long)a * small_const` writes `a >> log2(const)` into the high word instead of the correct `(a*const) >> 32`. Confirmed on real hardware via probe G (fixes113): `(long long)131072 * 1024LL` produced hi=128, lo=134217728 — full product 549,890,031,616 instead of 134,217,728. This broke every FDIV/FMUL in libcss for weeks and masqueraded as a layout bug. **Mitigation:** route 64-bit fixed-point math through `double` under `#ifdef __MWERKS__`. PPC has a hardware FPU and IEEE 754's 52-bit mantissa covers every int32 fixed-point intermediate. See [browser/netsurf/include/libcss/fpmath.h](browser/netsurf/include/libcss/fpmath.h) (fixes114) for the reference pattern. Pure int32 multiplies and divides are fine — the miscompile is specifically the 64-bit shift-multiply path. **Any code doing `int64_t` or `long long` fixed-point math on CW8 PPC is suspect** and needs the same treatment or a confirmation that operands stay small enough that the miscompilation is harmless (e.g. `INTTOFIX(128)` happened to work because `128 >> 10 = 0`, which is the correct hi word by coincidence).
- **Mac CR line endings** are required for all `.c` / `.h` / `.r` files in the project. Convert with `sed 's/$/\r/' | tr -d '\n'` before packaging.
- **TextEdit (`TENew` / `TEDispose`) crashes with dsMemWZErr if WRefCon is not initialized before the first call.** The crash happens because `GetWRefCon` returns garbage on a fresh window and TextEdit dereferences it. Safe pattern: `SetPort(window)` then `SetWRefCon(window, 0)` (or to a valid struct pointer) before calling `TENew`. Once this is set, TextEdit is fully usable for the URL field and other text input widgets.
- **`kWindowStandardHandlerAttribute`** intercepts update events and leaves windows blank. Do not pass it to `CreateNewWindow`.
- **Synchronous `browser_window_schedule_reformat` during resize causes infinite layout loops.** Never call reformat directly from the grow box handler. Instead set a `needs_reformat` flag on `struct macsurf_window` and handle it in the next `nullEvent` pass. Add a `reformat_in_progress` re-entrancy guard that logs and returns if a reformat call arrives while one is already running.
- **TextEdit field activation requires explicit `TEActivate` on window activation and `TEIdle` on every `nullEvent` pass** for the caret to blink and the field to accept keystrokes. `TEKey` must be gated by a `url_field_active` flag so Return and Escape don't accidentally route as typed characters.
- **libcss lexer tokenizes `--foo` as two tokens without the keystone fix.** The original CSS 2.1 grammar allowed one leading dash for vendor prefixes. Custom properties use two. libcss's `CDCOrIdentOrFunctionOrNPD` state needs a branch where `--` followed by `startNMStart(c)` appends and continues into `IdentOrFunction` rather than rewinding to emit CHAR. Without this, the 19 `:root` definitions and 219 `var()` references in a typical modern theme drop at tokenization before any parser logic runs. Fixed in fixes139 ([browser/libcss/src/lex/lex.c](browser/libcss/src/lex/lex.c)).
- **Force-sticky title bar probes clobber each other, last writer wins.** If multiple rounds of code add `macsurf_debug_set_title_force` or `log_int_force` probes without stripping predecessors, the latest writer overwrites everything earlier in the same reformat cycle. Non-force `MS_LOG` cycling through different labels (e.g. `plot rect ↔ plot clip`) indicates no sticky is latched, which usually means the expected code path is dead. Strip upstream stickies before adding new diagnostics.
- **Fix zips only refresh the files they ship.** If a diagnostic probe was added to file X in an earlier round and subsequent zips don't ship X, the probe persists on the Mac across rounds even after removal from the Linux tree. Phantom output with no Linux-grep hit means the Mac copy of the file is out of sync with Linux. Ship the affected file explicitly to resync (fixes137 did this for `html.c` / `layout.c` / `box_construct.c`).
- **`row-gap` shares computed-style storage with `column-gap` (fixes148).** `css__parse_gap` and `css__parse_row_gap` both emit `CSS_PROP_COLUMN_GAP` bytecode. Consequences: (a) single-value `gap: N` and standalone `row-gap: N` both work as expected because they set one value the layout reads on both axes; (b) two-value `gap: A B` loses the first value — column-gap ends up equal to `B` and `A` is discarded; (c) `css_computed_row_gap` accessor does NOT exist — `layout_flex.c` reads `css_computed_column_gap` for both axes (`ctx->main_gap` and `ctx->cross_gap` both derive from it). Full-fidelity split requires adding CSS_PROP_ROW_GAP as a real property: ~17 files incl. the bit-packed `autogenerated_computed.h` struct, where a new field must be allocated in a free bit slot (word 14 has ~16 free bits). That work was scoped and deferred — it is not a 1-round change because offset miscounts in propset.h/propget.h silently corrupt unrelated properties. Defer until a dedicated bit-packing audit round.
- **UPP macro override on CarbonLib is unsafe — don't cast function pointers to UPPs.** CarbonLib's `TrackControl` / `InstallEventHandler` / etc. dispatch action procs through MixedMode: the UPP argument is expected to be a **RoutineDescriptor** (pre-Carbon style, still used by CarbonLib 1.x), not a raw PPC function pointer. Overriding `NewControlActionUPP(proc)` to `((ControlActionUPP)(proc))` — on the theory that "Carbon UPPs are just native function pointers" — is correct only for Mach-O Carbon.framework on Mac OS X, **not** for CarbonLib on OS 9. Under CarbonLib, MixedMode reads "descriptor fields" from the function-entry bytes, resolves a routine address (typically 0 or garbage), and executes `bl 0`. Crash signature: PC in very low memory (e.g. `0x00000008`) with LR matching (e.g. `0x00000004`), because `bl` at address 0 sets LR=4 and CPU walks forward through low-memory globals until first illegal opcode. CurApName is often **`CodeWarrio...`** because CW runtime captures the low-memory trap. The override in `browser/netsurf/frontends/macos9/window.c` was removed in fixes147 after it caused scroll-bar clicks to crash on every attempt. **Correct approach:** avoid the action-UPP path entirely — call `TrackControl(ctrl, pt, NULL)` and respond on return using the `ControlPartCode` from `FindControl` plus `GetControlValue()` for live-tracking CDEFs. Or if a UPP is genuinely required, let CW8's Universal Interfaces expand the macro normally (CarbonLib does export `NewRoutineDescriptor` / `DisposeRoutineDescriptor`; they are deprecated in Mach-O Carbon but present and functional in CarbonLib). See [browser/netsurf/frontends/macos9/window.c](browser/netsurf/frontends/macos9/window.c) `macos9_window_handle_scrollbar_click`.
- **Appearance live-tracking scroll bar CDEF (`kControlScrollBarLiveProc = 386`) is unsafe on real G3/G4 hardware.** `TrackControl(ctrl, pt, NULL)` on a proc-386 control crashed into MacsBug with IDENT pointing at an Appearance Manager internal symbol (`hD*` prefix). The crash is not reproducible in SheepShaver — emulated CarbonLib tolerates the same call path that real hardware rejects. fixes147's UPP macro override removal did not close the crash; fixes159 swapped to the non-live Appearance CDEF (`kControlScrollBarProc = 384`), which is crash-free because its CDEF does not do per-frame app callbacks during thumb drag. The live-track path that proc 386 uses on real hardware appears to corrupt or dispatch through bad state that only manifests outside the emulator. **If a new Control Manager feature needs live-track behavior, do NOT reach for proc 386 without a reproducer and a MacsBug trace** — the crash is hardware-specific, not reproducible from Linux, and the only path forward is direct-hardware `wh`/`sc`/`ip` capture to identify the corrupted state inside the CDEF. See [browser/netsurf/frontends/macos9/window.c](browser/netsurf/frontends/macos9/window.c) near `macos9_window_handle_scrollbar_click` and the two `NewControl` calls for the current setup.
- **Carbon event classes have per-environment availability — check Apple's `CarbonEvents.h` before registering any handler.** Events added in Mac OS X 10.0+ that were never back-ported to CarbonLib (e.g. `kEventMouseWheelMoved`) will register without error but never dispatch, and CarbonLib's dispatcher destabilizes when something downstream tries to deliver an event whose class it doesn't know. The handler code will look correct in review (pascal calling convention, proper UPP, initialized EventTypeSpec, explicit return paths — all five "classic bugs" can be absent), run in hardware tests as "no crash from our code," and get blamed for illegal-instruction crashes at heap-looking addresses that are actually CarbonLib walking uninitialized dispatch state. Apple's `CarbonEvents.h` marks each event enum with either `Mac OS X: 10.x+ in Carbon.framework` AND `CarbonLib: 1.x+`, or `CarbonLib: not available` — respect the annotation. If `CarbonLib: not available`, the platform cannot deliver that event at all, and the correct fix is to not install a handler, not to debug the handler. See [browser/netsurf/frontends/macos9/macos9_wheel.c](browser/netsurf/frontends/macos9/macos9_wheel.c) and [docs/usb-overdrive.md](docs/usb-overdrive.md) for the wheel-event case study (fixes134 → fixes140).

## CLAUDE.md Maintenance

**This file must be kept current. It falls out of date fast when not actively maintained, and stale context causes agents to repeat solved problems.**

Update CLAUDE.md as part of every round that changes project state:

- When a blocker is resolved, remove it from "Known Issues" or "Current Blocker" immediately
- When a new class of bug is identified (like CW8 PPC `long long` miscompile), add it to "Known Gotchas" with the concrete reference pattern
- When a new subsystem lands (JS engine, chrome, image handlers), add a top-level section for it
- When a file count or project structure changes materially, update the "Project File List" section
- When the build state advances (v0.2 → v0.3 etc.), update the "Build State" section

The goal is that any new agent reading CLAUDE.md at the start of a session has an accurate picture of where the project actually stands — not where it was three rounds ago. If the file has drifted from reality, fix it before doing any new work.