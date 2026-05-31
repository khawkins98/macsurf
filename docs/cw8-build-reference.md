# CW8 Build Reference

Reference data for the CodeWarrior 8 build of MacSurf. Lists of access paths, file counts, ported library status, and the C89 audit checklist for porting new libraries. CLAUDE.md links here from "Build Environment".

For the build setup walk-through (installing CW8, opening the project for the first time), see [codewarrior-setup.md](codewarrior-setup.md).

---

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

### Project File List (470 .c files)
Added to MacSurf.mcp:
- 12 frontend `.c` files
- 5 shim `.c` files
- 10 NetSurf core `.c` files (utils + content + desktop)
- 15 libparserutils
- 30 libhubbub
- 95 libdom
- 303 libcss

`MacSurf.rsrc` (pre-compiled binary fork carrying `'carb'` + icon family + FREF + BNDL — see "Carbon App Requirements" above and [docs/resources.md](docs/resources.md)) must also be in the project; CW8 links `.rsrc` files directly into the output resource fork with no Rez step. The `*_stub.c` files exist on disk but are NOT in the project file list. See [docs/research/architecture-inventory.md](docs/research/architecture-inventory.md) for the full breakdown.


### Library Dependency Chain, COMPLETE

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

**Next milestone, NetSurf core wiring (5 phases).** All five libraries are now ported and C89-clean (443 .c files in MacSurf.mcp), so the remaining work is glue between NetSurf core and the libraries. Full audit and sequencing in [docs/research/netsurf-core-wiring.md](docs/research/netsurf-core-wiring.md). Phases:

1. **HTTP fetcher rewrite**, `macos9_http_fetcher.c` implementing the real `fetcher_operation_table`, replacing the v0.1 standalone OT fetch path. Reuses the OT primitives from `macos9_fetch.c`. Delete `fetch_stub.c`.
2. **Content handler infrastructure**, add `content/content_factory.c` + ~9 utils/ helpers (corestrings, libdom, talloc, hashtable, idna, etc.) + ~4 desktop/ helpers (selection, scrollbar, textarea, system_colour). Cascading compile errors expected.
3. **CSS handler**, add 5 files from `content/handlers/css/`, convert designated initializers, delete `frontends/macos9/css/` stubs.
4. **HTML handler**, add 23 files from `content/handlers/html/`, convert ~9 designated initializers (including the `html_content_handler` vtable with 16+ function pointer fields), 1 for-scope decl in `layout_flex.c`, delete `frontends/macos9/html/` stubs, create `dom/bindings/hubbub/parser.h` wrapper header.
5. **End-to-end render**, implement `plot_text` / `plot_clip` / `plot_rectangle` in QuickDraw in `plotters.c`, wire `browser_window_create` to drive a real fetch through `hlcache_handle_retrieve`.

**Total scope:** ~44 new .c files in MacSurf.mcp (taking the project to ~487 files), ~800 lines of new frontend code, ~25 designated init conversions, 226 lines of stub deletion. **Image rendering deferred to v0.3**, `image_init()` is fully `#ifdef WITH_*` gated, so without `WITH_BMP`/`WITH_GIF`/etc. it's a no-op and saves 28 files / 5.4K LOC of work this milestone.

**Most likely bottleneck:** the talloc question. NetSurf's `utils/talloc.c` is a Samba-derived hierarchical allocator with POSIX-y patterns; if it doesn't compile under CW8 it needs its own port pass before HTML can land. Documented in §13 open question 3 of the wiring audit.

### Library port audit checklist

When auditing a new C99 library for CW8 / strict C89, grep for:
- `inline` keyword
- `//` line comments (start-of-line AND trailing, but EXCLUDE URLs in `/* */` block comments, especially `http://www.opensource.org/licenses/...`)
- C99 designated initializers (`^\s*\.\w+\s*=`), and **count instances per file**, not just file count. format_list_style.c had 47 in one file.
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
- **GNU union casts**, `(union_type)0` or `(typedef_name)expr` where the typedef resolves to a union. Standard C89 forbids casting to union types. The libcss audit missed 5 sites of `(css_fixed_or_calc)0`.
- **Union initializers using designated syntax**, `{.field = value}` for a typedef'd union looks identical to a struct designated init in grep output. C89 union initializers must use `{value}` (positional, first member only).
- Build-time codegen (`gperf`, perl scripts, `.inc` files included from `.c` files)
- Existing MacSurf stubs in `frontends/macos9/<libname>/` that will conflict with the real headers
