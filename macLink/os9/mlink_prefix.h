/*
 * mlink_prefix.h — CW8 prefix file for macLink-Proxy
 *
 * Set as the project's prefix file in CW8 (Target Settings → C/C++
 * Language → Prefix File). Injected before every translation unit.
 *
 * Mirrors MacSurf's macsurf_prefix.h conventions:
 *   - __MACOS9__ defined for code that needs Mac-only paths
 *   - NO_IPV6 — we're IPv4 only on this platform
 *   - TARGET_API_MAC_CARBON for Carbon code paths
 *   - MacTypes.h first to prevent bool/true/false collisions
 */
#ifndef MLINK_PREFIX_H
#define MLINK_PREFIX_H

#include <MacTypes.h>

#ifndef __MACOS9__
#define __MACOS9__              1
#endif

#ifndef NO_IPV6
#define NO_IPV6                 1
#endif

/* CW8's Carbon target panel already defines this; guard so we don't
 * collide if the project settings panel was already used to set it. */
#ifndef TARGET_API_MAC_CARBON
#define TARGET_API_MAC_CARBON   1
#endif

/* OS-level Carbon include is gated to a single place so future
 * subheader-collision suppression (per CLAUDE.md notes on Carbon.h
 * sub-header guards) goes here. */

#endif /* MLINK_PREFIX_H */
