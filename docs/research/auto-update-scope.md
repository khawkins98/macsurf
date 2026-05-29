# Auto-update checker — scope

**Status:** scoping doc, no code yet.
**Goal:** MacSurf can check whether a newer release is available, tell the user, and let them download the new `MacSurf.sit`. Toggle in preferences. Privacy-safe defaults. No auto-replace of the running binary.

---

## Product shape

What the user sees:

1. **A new menu item under Apple → "Check for Updates..."** that does a manual check on demand.
2. **An on-launch background check** (a few seconds after the home page settles), gated by a preference.
3. **When a newer version exists**, a Carbon `StandardAlert` with three buttons:
   - **Download** — opens NavPutFile, user picks where to save the new `.sit`, MacSurf streams it down via macTLS using the existing download manager (fixes313), progress dialog shows, file lands on disk. User expands and replaces manually.
   - **Not Now** — dismiss. We'll re-prompt next launch (or next manual check).
   - **Skip This Version** — write the version string to prefs so we don't bother the user about *this specific version* again. Still notify on the version after it.
4. **A new Preferences dialog** (or a single checkbox surfaced somewhere accessible) with **"Check for updates on launch"**, default ON.
5. **When no update / network failure**, the on-launch check stays completely silent. Manual menu invocation shows a small "MacSurf is up to date" alert OR a "Could not reach update server" alert — both polite, single-button.

What the user *does not* see: any tracking, any analytics ping, anything that requires a network connection to use the browser.

---

## Server side — `home.macsurf.org/latest.txt`

A tiny static text file we control, fetched over HTTPS via macTLS. **Not the GitHub Releases API** — no JSON parsing, no rate limits, no third-party dependency on the data path. We already control home.macsurf.org and it already serves over the same TLS stack the browser uses.

Format (line-based, simple to parse in C89):

```
version: 1.3.1
sit: https://github.com/mplsllc/macsurf/releases/download/v1.3.1/MacSurf.sit
notes: https://github.com/mplsllc/macsurf/releases/tag/v1.3.1
codename: Forward, refined
date: 2026-05-29
```

- One `key: value` per line, keys case-insensitive, leading/trailing whitespace stripped.
- Unknown keys ignored (forward-compatible).
- Only `version` and `sit` are required. Everything else is optional and shown if present.
- File served with `Content-Type: text/plain`, `Cache-Control: max-age=600` (10 min CDN cache OK; manual check still hits fresh server on URL bar manipulation if needed).

Operational cost: zero. Edit the file at each release. We could even regenerate it from the GitHub release metadata via a tiny PHP script on home.macsurf.org so the manual step is automatic, but the file itself stays plain text for the client.

**Why not the GitHub API:** Writing a JSON parser in C89 for this is gratuitous (~400 lines). The GitHub API would also rate-limit unauthenticated clients to 60/hour per IP, which for a single user is fine but adds a failure mode we don't need. Our static file has none of those concerns.

---

## Client side — components

### 1. `macos9_version.c` / `macos9_version.h`

The single source of truth for the build's version. Replaces the four inconsistent User-Agent strings currently floating in the fetchers (`MacSurf/0.1`, `0.2`, `0.3`).

```c
/* macos9_version.h */
#define MACSURF_VERSION_MAJOR  1
#define MACSURF_VERSION_MINOR  3
#define MACSURF_VERSION_PATCH  1
#define MACSURF_VERSION_STRING "1.3.1"
#define MACSURF_USER_AGENT     "MacSurf/1.3.1 (Macintosh; PPC Mac OS 9)"

/* Parses "1.3.1" into (1, 3, 1). Returns 0 on success.
 * Accepts missing patch ("1.3" → 1.3.0). */
int macsurf_version_parse(const char *s, int *maj, int *min, int *pat);

/* Returns -1, 0, +1 (a < b, a == b, a > b). Parses both strings.
 * Treats unparseable input as version 0. */
int macsurf_version_compare(const char *a, const char *b);
```

~80 lines C89. Independent of everything else; testable in isolation. Should land first; the User-Agent unification alone is overdue cleanup.

### 2. `macos9_prefs.c` / `macos9_prefs.h`

A minimal preferences file in `kPreferencesFolderType / "MacSurf Prefs"`. Currently MacSurf has no such file — preferences exist only in `nsoption` (NetSurf core in-memory) and built-in constants. This adds persistent storage for browser-frontend choices.

**Initial scope:** just the update-check prefs. Add more keys as future work.

File format: same line-based `key: value` style as `latest.txt`. Trivial to read, trivial to write, human-readable in BBEdit if needed.

```
update_check_enabled: 1
update_skip_version: 1.4.0
update_last_check_unix: 1748544000
```

API:

```c
/* macos9_prefs.h */
int  macsurf_prefs_load(void);    /* called once at startup */
int  macsurf_prefs_save(void);    /* called when a pref changes */

int  macsurf_prefs_get_bool(const char *key, int default_value);
void macsurf_prefs_set_bool(const char *key, int value);
const char *macsurf_prefs_get_string(const char *key);
void macsurf_prefs_set_string(const char *key, const char *value);
long macsurf_prefs_get_long(const char *key, long default_value);
void macsurf_prefs_set_long(const char *key, long value);
```

Backing store is a single in-memory `struct macsurf_pref_entry { char key[32]; char value[256]; }` array, capped at maybe 32 entries (we only need ~10). On `set`, mark dirty + schedule save. On `load`, scan once and populate the array. Lost prefs (file missing / unreadable) silently fall back to defaults.

~200 lines C89. Touches Files Manager (`FSpCreate`, `FSpOpenDF`, `FSWrite`, `FSClose`) and Folder Manager (`FindFolder(kPreferencesFolderType)`). Both are well-trodden ground; nothing exotic.

### 3. `macos9_update_check.c` / `macos9_update_check.h`

The actual check + parse logic.

```c
/* macos9_update_check.h */
struct macsurf_update_info {
    char version[32];       /* "1.3.2" */
    char sit_url[512];      /* https://.../MacSurf.sit */
    char notes_url[512];    /* https://.../releases/tag/... (optional, "" if absent) */
    char codename[64];      /* optional, "" if absent */
    char date[16];          /* optional, "" if absent */
    int  is_newer;          /* 1 if remote > current */
};

/* Start an async check. Pass NULL completion to make it silent (the
 * launch-time auto check); pass a callback for the menu-triggered
 * manual case that wants to show "up to date" alerts.
 *
 * The check is best-effort: any failure (network, TLS, parse, no
 * latest.txt at all) is silently absorbed in the silent path.
 *
 * Triggered on launch ~5 seconds after the home page settles, gated
 * by macsurf_prefs_get_bool("update_check_enabled", 1). Menu-trigger
 * ignores the pref. */
void macsurf_update_check_start(int silent);

/* Polled from the event loop, like the fetchers. Drives macTLS
 * forward and dispatches the result. Idempotent if no check in
 * flight. */
void macsurf_update_check_poll(void);

/* Returns 1 if a result is ready and writes it to *out. Returns 0
 * otherwise. The caller (UI code) is the one that surfaces the
 * dialog. */
int macsurf_update_check_take_result(struct macsurf_update_info *out);
```

Implementation:

- Reuse the existing HTTPS fetcher (`macos9_https_fetcher.c`). Spin up a fetch context against `https://home.macsurf.org/latest.txt`. The fetcher already handles macTLS, OT, keep-alive, all of it.
- Body is small (~150 bytes); single in-memory buffer of 4 KB is fine. No streaming needed.
- On FETCH_FINISHED with status 200, parse the body line by line. Populate `struct macsurf_update_info`.
- On any failure, just clear the in-flight flag and bail. No logging beyond a single MS_LOG line for forensics.
- Compute `is_newer = macsurf_version_compare(remote.version, MACSURF_VERSION_STRING) > 0`.
- If `is_newer && skip_version != remote.version`, post the result so the UI code can pick it up next event loop iteration.
- Skip-version check uses `macsurf_prefs_get_string("update_skip_version")`. Empty / no-pref = no skip.

~250 lines C89.

### 4. UI wiring — `main.c` + `window.c`

**Menu wiring (`main.c`):**

- Add `kMacSurfMenuCheckForUpdates = 50` (or wherever the next slot is) to the menu enum.
- After `AppendMenu(apple_menu, "\pAbout MacSurf...")`, add `AppendMenu(apple_menu, "\p(-")` and `AppendMenu(apple_menu, "\pCheck for Updates...")`.
- In the existing menu-dispatch switch, route the new item ID to `macsurf_update_check_start(0 /* not silent */)`.

**Launch-time check (`main.c`):**

- After the home page is fetched and the first reformat completes, set a small timer (e.g. 300-tick = 5-second TickCount delta tracked in the WaitNextEvent loop) so the check doesn't fight the cold-load.
- When the timer expires AND `macsurf_prefs_get_bool("update_check_enabled", 1)`, call `macsurf_update_check_start(1 /* silent */)`.
- Once per session — set a `static int g_did_launch_check = 0;` flag.

**Polling (`main.c`):**

- Call `macsurf_update_check_poll()` once per WaitNextEvent pass, same place we poll the fetchers.
- Call `macsurf_update_check_take_result(&info)` — if returns 1, dispatch to the dialog code.

**Dialog (`window.c` or new `macos9_update_dialog.c`):**

- Carbon `NoteAlert` / `StandardAlert` (CarbonLib-safe — already used by MacSurf?). Three buttons: **Download** (default), **Skip This Version**, **Not Now** (cancel).
- Build the message string: `"MacSurf %s (%s) is available.\nYou are running version %s.\n\nRelease notes: %s"`. Codename / notes URL omitted if absent.
- On **Download** click: kick off the download via the existing fixes313 download manager. The download URL (`info.sit_url`) is the source; NavPutFile picks the dest.
- On **Skip This Version** click: `macsurf_prefs_set_string("update_skip_version", info.version)` + `macsurf_prefs_save()`.
- On **Not Now** click: nothing persistent; next launch will re-prompt if still newer than current.

For "MacSurf is up to date" (manual-check path only): single-button `NoteAlert`. For "Could not reach update server": single-button `CautionAlert`.

### 5. Download integration — reuse fixes313

The existing `macos9_download.c` exposes the `gui_download_table` that NetSurf core calls. To kick off a download from outside NetSurf (i.e. from the dialog's Download button), we have two options:

- **A. Use NetSurf core's `browser_window_download`** with the URL. Core handles the fetch, dispatches to our `gui_download_table` callbacks, which already do NavPutFile + FSWrite streaming.
- **B. Build a synthetic `gui_download_window` ourselves**, open the HTTPS fetcher directly, drive the data into FSWrite without going through NetSurf core.

A is way less code and reuses everything; the download dialog/progress already works. ~10 lines of glue. Use it.

---

## Failure modes

| Failure | Silent (launch) path | Manual (menu) path |
|---|---|---|
| Network down | No alert. Single MS_LOG line. | "Could not reach update server" alert. |
| macTLS handshake fails | Same as above. | Same as above. |
| HTTP 404 on `latest.txt` | Same. | Same. |
| Parse error / malformed file | Same. | "Could not reach update server" alert (don't expose parse details to the user). |
| Server says version equal | Nothing. | "MacSurf is up to date" alert. |
| Server says version older | Nothing (we trust the server but never downgrade-prompt). | "MacSurf is up to date" alert. |
| Server says version newer, user skipped this version | Nothing. | Prompt anyway (manual implies override). |
| Server says version newer, user hasn't skipped | Show update prompt. | Show update prompt. |
| Download itself fails | Existing download manager surfaces the error. | Same. |

---

## Privacy & security

- **Every check is one HTTPS GET to `home.macsurf.org/latest.txt`** with our normal User-Agent. Server log captures IP + UA. Document this in the README — small project, no analytics, but be honest about the network call.
- **Integrity:** macTLS verifies home.macsurf.org's cert against the 121-anchor bundle. The `latest.txt` URL is a literal in the binary, so DNS poisoning of `home.macsurf.org` doesn't help an attacker — they'd also need to forge a valid cert. Acceptable for "show user a URL to download from."
- **The `.sit` file itself is fetched from GitHub Releases** over HTTPS, again via macTLS. No code signing layer below that (Classic Mac OS has no native code-signing infrastructure). Users can verify SHA-256 manually if they care; we should publish the hash in `latest.txt` as a future field.
- **No telemetry collected.** We don't track which versions are running. Nothing about the response affects future requests — the server doesn't know we read it; only that an IP requested the file.
- **Default for the toggle:** ON. Justification: keeping users on outdated TLS / security fixes is a worse outcome than the privacy cost of one HTTPS request to a server we already operate. The toggle is prominent in prefs for anyone who wants it off.

---

## Phasing

| Phase | Files | LOC est. | Depends on |
|---|---|---:|---|
| **1. Version module** | `macos9_version.c/.h` + update User-Agent strings in 4 fetchers | ~120 | none |
| **2. Server-side `latest.txt`** | One file on `home.macsurf.org` | n/a | none |
| **3. Prefs storage** | `macos9_prefs.c/.h` | ~200 | none |
| **4. Update check core** | `macos9_update_check.c/.h` | ~250 | 1, 3 |
| **5. Menu + launch wiring** | `main.c` edits | ~50 | 4 |
| **6. Dialog** | `macos9_update_dialog.c` (or inline in `window.c`) | ~120 | 4, fixes313 |
| **7. Optional: Prefs UI** | `macos9_prefs_dialog.c` (or single checkbox somewhere accessible) | ~100 | 3 |

**Total new C89:** ~700 lines, plus the User-Agent-string unification cleanup which simplifies what's already there.

**MacSurf.mcp additions:** 4-5 new `.c` files. No Access Paths changes (all in `frontends/macos9/`).

**No new libraries.** No new BearSSL. No JSON parser. No NetSurf core changes.

Phases 1, 2, 3 can land in any order; they're independent. Phase 4 needs 1 and 3. Phases 5, 6, 7 need 4. Phase 7 is optional for the first ship — a single hidden-but-documented pref key is acceptable if we want to skip the dialog work initially.

**Recommended first cut:** ship phases 1-6 as one round. Skip phase 7 (no prefs dialog yet); users edit the prefs file manually if they want to disable, and the default-ON behavior is fine for the first release. Add the dialog later if there's demand.

---

## Open questions

- **Carbon `StandardAlert` vs `NoteAlert` vs custom dialog**: which is MacSurf already using for prompts? (None that I've seen in source — confirm.) Pick whichever path has the least toolbox surface.
- **`http://home.macsurf.org/latest.txt` fallback** if HTTPS fails: probably not. We trust the cert verification; falling back to plain HTTP would let an attacker forge the response and direct users to a malicious `.sit`. Better to silently fail than degrade security.
- **GitHub Releases API as a secondary source** if `home.macsurf.org` is down: also probably not. We control home.macsurf.org and it's been stable; adding GitHub API as a fallback would require JSON parsing, which we explicitly want to avoid.
- **Should the User-Agent unification land first as its own commit/round?** Probably yes — it's a small, clean, regression-friendly change that stands on its own merit and doesn't need to wait for the update checker.

---

## Out of scope (deferred)

- **In-place update / auto-relaunch.** Classic Mac OS makes this hairy (Resource Manager, FInfo manipulation, deleting a running file, relaunch via Process Manager). Not worth the risk; downloading the `.sit` and letting the user replace manually is the safe path.
- **SHA-256 verification of the downloaded `.sit`.** Useful, queued for a follow-up. Requires hash field in `latest.txt` (already designed forward-compatible) and a small SHA-256 step over the downloaded file (BearSSL primitives already available). ~50 lines.
- **Update history dialog ("what's new in 1.3.1?").** Show release notes inline. Could just open `notes_url` in a new browser window, which is one line of code if we want it.
- **Beta channel toggle.** Pref-gated, fetches `latest-beta.txt` instead. Trivial to add but no current need.
- **Tracking of "I dismissed this version N times".** If we wanted to escalate UI urgency for users ignoring updates. Probably overkill.

---

## Recommended ship pattern

1. **Round A (User-Agent unification + version module)**: Phase 1 alone, separate commit. Self-justifying cleanup, immediate value.
2. **Round B (prefs + update checker + menu + dialog)**: Phases 3, 4, 5, 6 together. Server-side `latest.txt` (Phase 2) goes live at the same time. Ship a tar with the new `.c` files; mention `macos9_version.c`, `macos9_prefs.c`, `macos9_update_check.c`, and optionally `macos9_update_dialog.c` for the .mcp additions.
3. **Round C (optional polish)**: Prefs dialog, SHA-256 verify, anything else from the deferred list.

Each round is independently shippable and hardware-verifiable. Round B is the actual feature; round A is the cleanup that should land first to keep round B's diff focused.
