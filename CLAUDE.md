# MacSurf

A lightweight web browser for Mac OS 9 PowerPC, built on the NetSurf engine, paired with a simple TLS proxy.

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

- Target: Power Mac G3/G4, Mac OS 9.1-9.2.2, minimum 64MB RAM
- Compiler: CodeWarrior 8 (on-machine) or cross-compile GCC PPC from Linux
- No threading — OS 9 is cooperative multitasking, use WaitNextEvent loop
- No HTTPS in browser — all TLS handled by proxy
- No JavaScript — by design, keeps memory footprint low
- Carbon API for UI — works on OS 9 and early OS X

## Reference Frontends

NetSurf's RISC OS and AmigaOS frontends are the primary references — both solved cooperative multitasking on non-POSIX systems. Study these before writing any frontend code.

- `frontends/riscos/` — closest analog to Mac OS 9
- `frontends/amiga/` — also cooperative multitasking

## Coding Conventions

- C for browser frontend (matches NetSurf codebase)
- Go for proxy
- Keep Mac Toolbox calls isolated in their own files (window.c, bitmap.c, font.c etc.)
- No external dependencies in proxy — stdlib only
- Every Open Transport call must be async — never block the event loop

## Proxy Protocol

Standard HTTP proxy protocol — no custom protocol. The Mac sends a normal HTTP proxy request, the proxy fetches via HTTPS and returns plain HTTP. Compatible with any browser that supports HTTP proxies (Classilla works today as validation).

## Do Not

- Do not add JavaScript support
- Do not enable tabs by default
- Do not use preemptive threads anywhere in the browser
- Do not add external dependencies to the proxy
- Do not target OS X only — Carbon must run on OS 9