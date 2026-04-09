# MacSurf Build Error Report (Build 4)

**Date:** 2026-04-09
**Source:** `errors.txt` after access path fix (stubs moved to System paths)
**Total errors:** 675 (down from 1,293 — **618 eliminated**)

---

## Progress Across All Builds

| Build | Errors | Delta | Key Fix |
|-------|-------:|------:|---------|
| 1 | 1,374 | — | Baseline |
| 2 | 1,331 | -43 | time.c for-loops, PATH_MAX, Mac type guards, network stubs, ICE |
| 3 | 1,293 | -38 | NSLOG multi-arg in fs_backing_store/nsoption, mode_t/off_t |
| 4 | **675** | **-618** | **Access paths moved to System — libwapcaplet/dom/libcss/nsutils found** |
| **Total fixed** | | **-699** | **51% of original errors eliminated** |

The access path fix was the big one — 618 errors gone in one shot. The remaining 675 are real errors, not cascading from missing headers.

---

## Error Summary by Pattern

| Pattern | Count | Category |
|---------|------:|----------|
| `')' expected` | 103 | NSLOG multi-arg (new files now compiling) |
| `declaration syntax error` | 66 | Cascading from missing types/NSLOG |
| `';' expected` | 66 | NSLOG multi-arg cascade |
| `identifier expected` | 52 | Cascading |
| `illegal function definition` | 50 | Cascading |
| `macro 'O_*' redefined` | 152 | **fcntl.h / mac_types.h conflict** |
| `macro 'S_*' / struct stat redefined` | 40 | **stat.h / mac_types.h conflict** |
| `illegal name overloading` | 30 | Forward enum/type conflicts |
| `bool *` to `unsigned char *` | 17 | **bool type mismatch in lwc calls** |
| `unsigned long long *` to `unsigned long *` | 6 | **uint64_t size mismatch** |
| `struct hlcache_retrieval_ctx` incomplete | 13 | Missing header cascade |
| `cannot be opened` | 8 | **Missing content handler headers** |
| `struct tm` incomplete | 3 | **time.h shim missing struct tm** |
| `kTEC*` undefined | 4 | mac_iconv.c TEC include (already fixed, not deployed?) |
| Other | 65 | Various cascading |

---

## Errors by File

| File | Count | Primary Cause |
|------|------:|---------------|
| fcntl.h | 152 | O_RDONLY/O_WRONLY/O_RDWR/O_TRUNC redefined × 38 compilation units |
| browser_window.c | 92 | Missing html/html.h, form_internal.h, js.h + NSLOG multi-arg |
| llcache.c | 71 | NSLOG multi-arg + bool* conversion |
| log.c | 70 | struct utsname/timeval cascade + NSLOG |
| hlcache.c | 68 | NSLOG multi-arg + hlcache_retrieval_ctx incomplete |
| stat.h | 36 | S_IFDIR/S_IFREG/struct stat redefined |
| urldb.c | 34 | NSLOG multi-arg |
| llcache.h | 22 | Cascading from llcache types |
| content.c | 21 | NSLOG multi-arg + uint64_t mismatch |
| content_protected.h | 17 | Cascading |
| netsurf.c | 14 | Missing css/css.h, image/image.h, js.h, html.h, textplain.h |
| All others | ~48 | Various |

---

## Root Causes (7 issues)

### 1. O_RDONLY / O_WRONLY / O_RDWR / O_TRUNC Redefined (152 errors)

**The single biggest issue now.** MSL's `fcntl.h` defines `O_RDONLY=0x2`, `O_WRONLY=0x4`, `O_RDWR=0x1`, `O_TRUNC=0x0800`. Our `mac_types.h` also defines them (with `#ifndef` guards) as `O_RDONLY=0`, `O_WRONLY=1`, `O_RDWR=2`, `O_TRUNC=0x0400`.

The prefix file now includes `<stat.h>` and `<fcntl.h>` early, BUT our stub `shims/stat.h` is found first (it's on the system path before MSL). So the include chain is:
1. Prefix → `<fcntl.h>` → finds MSL's fcntl.h? Or our shim?
2. Our `mac_types.h` defines O_* with `#ifndef` guards
3. Later, MSL's fcntl.h redefines them

**Fix:** Remove O_RDONLY/O_WRONLY/O_RDWR/O_TRUNC definitions from `mac_types.h` entirely — MSL provides them. Or match MSL's exact values (0x2, 0x4, 0x1, 0x0800).

### 2. S_IFDIR / S_IFREG / S_ISDIR / S_ISREG / struct stat / S_I*USR Redefined (40 errors)

Same issue as #1 but for stat macros. Our `mac_types.h` and `shims/stat.h` define these, but MSL's `stat.h` also defines them.

**Fix:** Same approach — remove from our files or match MSL's values exactly.

### 3. NSLOG Multi-Arg in Newly-Compiling Core Files (~170 errors)

Now that headers are found, more core files compile further and hit NSLOG multi-arg calls. Files affected:
- `browser_window.c` (~24 NSLOG errors)
- `llcache.c` (~77 NSLOG errors)
- `hlcache.c` (~12 NSLOG errors)
- `urldb.c` (~34 NSLOG errors)
- `content.c` (~18 NSLOG errors)
- `netsurf.c` (~5 NSLOG errors)

**Fix:** Replace multi-arg NSLOG with `nslog_log()` in these 6 files (same approach as before).

### 4. Missing Content Handler Headers (8 errors, ~100+ cascading)

`browser_window.c` and `netsurf.c` include internal content handler headers that aren't in our build:
- `html/html.h` (2 × cannot be opened)
- `html/form_internal.h` (1)
- `javascript/js.h` (2)
- `css/css.h` (1)
- `image/image.h` (1)
- `image/image_cache.h` (1)
- `text/textplain.h` (1)

These are NetSurf's HTML/CSS/JS/image content handlers. They live in `content/handlers/` which isn't in our access paths.

**Fix:** Either:
- Add `content/handlers/` to user access paths
- Or create stub headers for these (since we're not implementing HTML/CSS/JS rendering yet)

### 5. bool* vs unsigned char* in lwc Calls (17 errors)

`lwc_string_caseless_isequal()` takes a `bool *` parameter, but CW8 sees `bool` (from libwapcaplet stub) as a different type than `unsigned char` (from MacTypes.h). Lines like:
```c
&match) == lwc_error_ok && match
```
produce `illegal implicit conversion from 'bool *' to 'unsigned char *'`.

**Fix:** Ensure the `bool` typedef in our `libwapcaplet/libwapcaplet.h` stub matches `unsigned char` exactly as MacTypes.h defines it.

### 6. uint64_t / unsigned long long Size Mismatch (6 errors)

`nsu_getmonotonic_ms()` returns via a `uint64_t *` parameter, but our `uint64_t` is `unsigned long long` while the function expects `unsigned long *`.

**Fix:** Check our `stdint.h` stub — `uint64_t` should be `unsigned long long` on PPC (8 bytes). The function signature may need a cast or the nsutils stub may need fixing.

### 7. struct tm Incomplete (3 errors)

`utils/time.c` line 114 uses `struct tm *tm = gmtime(&t)` but `struct tm` is not defined. Our time shims define `struct mac_tm` but the standard `struct tm` is expected.

**Fix:** Add `struct tm` definition to the `sys/time.h` stub or create a `<time.h>` shim that provides it.

---

## Prioritized Fix Order

| # | Fix | Errors Fixed |
|---|---|---:|
| **1** | Remove O_*/S_*/struct stat from mac_types.h — let MSL define them | **~192** |
| **2** | Replace multi-arg NSLOG in 6 newly-compiling core files | **~170** |
| **3** | Add content handler access path or create stub headers | **~100+** |
| **4** | Fix bool typedef consistency for lwc calls | **~17** |
| **5** | Fix uint64_t / struct tm type issues | **~9** |
| **6** | Fix mac_iconv.c TEC include (if not yet deployed) | **~4** |

### Projected Outcome

Fixes 1-3 should eliminate ~460+ errors, bringing the count to roughly **~200**. Fixes 4-6 would bring it under **~170**. The remaining errors would be further cascading and NSLOG calls in deeper files.

### Key Insight

The access path fix was transformative — 618 errors gone. The error landscape has completely changed. Previous builds were dominated by "header not found" cascades. Now we're seeing **real compilation errors**: type mismatches, missing content handlers, and NSLOG calls in files that never compiled far enough before. This is progress — these are solvable, concrete issues.
