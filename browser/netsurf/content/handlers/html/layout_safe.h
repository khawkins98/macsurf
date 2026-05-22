/*
 * Copyright 2026 MacSurf
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * fixes168 — Layout-wide dimension sanitizers.
 *
 * The CSS layout engine carries internal sentinels (AUTO == INT_MIN,
 * unresolved-height markers, etc.) through its data structures. These
 * are valid as algorithm states but they must never reach pixel
 * arithmetic, child-layout calls, min/max clamps, or bbox math —
 * once they do, the layout produces INT_MIN x or w values that crash
 * downstream (defensive clamp, plotters, redraw walker) or paint at
 * a single coordinate.
 *
 * Apple's heavy nested flex/grid/table trees were producing exactly
 * this class of corruption. fixes167 added flex-local sanitizers
 * (flex_safe_dim / flex_safe_fallback_dim); fixes168 promotes that
 * pattern to a shared layout-wide header consumed by block, flex,
 * grid, table.
 *
 * Helpers are static so each TU compiles its own copy — CW8 is happy
 * with this and the bodies are tiny. Anyone touching this file:
 * keep the rules cheap (single comparisons, no allocation) because
 * they fire on every dimension before every layout call.
 */

#ifndef NETSURF_HTML_LAYOUT_SAFE_H
#define NETSURF_HTML_LAYOUT_SAFE_H

#include <limits.h>

#ifndef AUTO
#define AUTO INT_MIN
#endif

/* Any dimension outside ±LAYOUT_SAFE_MAX is treated as garbage. The
 * largest realistic page-coordinate today is well under 200000 px
 * (see CLAUDE.md note on the defensive redraw clamp), so 1e6 gives
 * five orders of magnitude of headroom while keeping us inside int32
 * with room for safe addition. */
#define LAYOUT_SAFE_MAX  1000000

/* Hard cap on children iterated by a single fallback path. Mirrors
 * FLEX_MAX_ITEMS from fixes167; grid uses this too. */
#define LAYOUT_MAX_CHILDREN 512

/**
 * Returns 1 if v cannot be used as a pixel value (AUTO/INT_MIN,
 * absurdly large/negative). Otherwise 0.
 */
static int layout_dim_is_auto_or_bad(int v)
{
	if (v == AUTO) return 1;
	if (v == INT_MIN) return 1;
	if (v < -LAYOUT_SAFE_MAX) return 1;
	if (v > LAYOUT_SAFE_MAX) return 1;
	return 0;
}

/**
 * Sanitize a width before arithmetic. If v is auto/bad, fall back to
 * the containing-block width (also sanitized). If even that is bad,
 * return 0 — zero-width is the only universally-safe fallback for
 * width.
 */
static int layout_dim_sanitize_width(int v, int containing_width)
{
	if (!layout_dim_is_auto_or_bad(v)) return v;
	if (!layout_dim_is_auto_or_bad(containing_width))
		return containing_width;
	return 0;
}

/**
 * Sanitize a height for base-size measurement. If v is auto/bad,
 * return 0. Heights are content-driven during measurement, so a
 * 0-height start is correct; layout will resolve the real value
 * from content.
 */
static int layout_dim_sanitize_height_for_measure(int v)
{
	if (layout_dim_is_auto_or_bad(v)) return 0;
	return v;
}

/**
 * General arithmetic sanitizer. Replace any AUTO/bad value with 0
 * before adding it into a running total. Use this anywhere a
 * sentinel would poison a sum.
 */
static int layout_dim_sanitize_for_arithmetic(int v)
{
	if (layout_dim_is_auto_or_bad(v)) return 0;
	return v;
}

/**
 * Saturating add. Returns the clamped sum, never overflows past
 * ±LAYOUT_SAFE_MAX. AUTO/bad inputs are treated as 0.
 */
static int layout_dim_add_safe(int a, int b)
{
	int sa = layout_dim_sanitize_for_arithmetic(a);
	int sb = layout_dim_sanitize_for_arithmetic(b);
	int s;
	/* Overflow check: if same sign and magnitudes large, clamp. */
	if (sa > 0 && sb > 0 && sa > LAYOUT_SAFE_MAX - sb)
		return LAYOUT_SAFE_MAX;
	if (sa < 0 && sb < 0 && sa < -LAYOUT_SAFE_MAX - sb)
		return -LAYOUT_SAFE_MAX;
	s = sa + sb;
	if (s > LAYOUT_SAFE_MAX) return LAYOUT_SAFE_MAX;
	if (s < -LAYOUT_SAFE_MAX) return -LAYOUT_SAFE_MAX;
	return s;
}

/**
 * Clamp any int dimension into the safe range, replacing AUTO/bad
 * with 0. Use at boundaries where a sentinel must not escape (bbox
 * assignment, paint-rect dispatch).
 */
static int layout_dim_clamp(int v)
{
	if (layout_dim_is_auto_or_bad(v)) return 0;
	return v;
}

#endif /* NETSURF_HTML_LAYOUT_SAFE_H */
