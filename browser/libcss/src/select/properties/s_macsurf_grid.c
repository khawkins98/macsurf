/*
 * This file is part of LibCSS
 * Licensed under the MIT License,
 *		  http://www.opensource.org/licenses/mit-license.php
 * Copyright 2026 MacSurf (fixes75, fixes118)
 */

#include <stdlib.h>
#include <string.h>

#include "bytecode/bytecode.h"
#include "bytecode/opcodes.h"
#include "select/propset.h"
#include "select/propget.h"
#include "utils/utils.h"

#include "select/properties/properties.h"
#include "select/properties/helpers.h"

#define MACSURF_GRID_TRACK_MAX 8

/* Allocate an 8-int track array initialised from `src` (which may be
 * NULL or all-zero, in which case we return NULL to keep memory tight).
 * Returns NULL on OOM as well; the caller treats NULL as "no tracks". */
static int32_t *macsurf__alloc_tracks(const int32_t *src)
{
	int32_t *out;
	int i;
	bool any = false;

	if (src == NULL) return NULL;
	for (i = 0; i < MACSURF_GRID_TRACK_MAX; i++) {
		if (src[i] != 0) { any = true; break; }
	}
	if (!any) return NULL;

	out = (int32_t *)malloc(MACSURF_GRID_TRACK_MAX * sizeof(int32_t));
	if (out == NULL) return NULL;
	for (i = 0; i < MACSURF_GRID_TRACK_MAX; i++) {
		out[i] = src[i];
	}
	return out;
}

static void macsurf__replace_tracks(css_computed_style *style,
		const int32_t *src)
{
	if (style->macsurf_grid_tracks != NULL) {
		free(style->macsurf_grid_tracks);
		style->macsurf_grid_tracks = NULL;
	}
	if (src != NULL) {
		style->macsurf_grid_tracks = macsurf__alloc_tracks(src);
	}
}

css_error css__cascade_macsurf_grid(uint32_t opv, css_style *style,
		css_select_state *state)
{
	uint16_t value = CSS_MACSURF_GRID_INHERIT;
	int32_t packed = 0;
	int32_t tracks[MACSURF_GRID_TRACK_MAX];
	int i;

	for (i = 0; i < MACSURF_GRID_TRACK_MAX; i++) tracks[i] = 0;

	if (hasFlagValue(opv) == false) {
		switch (getValue(opv)) {
		case 0x0000: /* NONE */
			value = CSS_MACSURF_GRID_NONE;
			break;
		case 0x0080: /* SET */
			value = CSS_MACSURF_GRID_SET;
			packed = *((css_fixed *) style->bytecode);
			advance_bytecode(style, sizeof(css_fixed));
			for (i = 0; i < MACSURF_GRID_TRACK_MAX; i++) {
				tracks[i] = *((css_fixed *) style->bytecode);
				advance_bytecode(style, sizeof(css_fixed));
			}
			break;
		}
	}

	if (css__outranks_existing(getOpcode(opv), isImportant(opv), state,
			getFlagValue(opv))) {
		css_error err = set_macsurf_grid(state->computed, value,
				packed);
		if (err != CSS_OK) return err;
		macsurf__replace_tracks(state->computed,
				value == CSS_MACSURF_GRID_SET ? tracks : NULL);
	}

	return CSS_OK;
}

css_error css__set_macsurf_grid_from_hint(const css_hint *hint,
		css_computed_style *style)
{
	css_error err = set_macsurf_grid(style, hint->status,
			hint->data.integer);
	if (err != CSS_OK) return err;
	macsurf__replace_tracks(style, NULL);
	return CSS_OK;
}

css_error css__initial_macsurf_grid(css_select_state *state)
{
	css_error err = set_macsurf_grid(state->computed,
			CSS_MACSURF_GRID_NONE, 0);
	if (err != CSS_OK) return err;
	macsurf__replace_tracks(state->computed, NULL);
	return CSS_OK;
}

css_error css__copy_macsurf_grid(
		const css_computed_style *from,
		css_computed_style *to)
{
	int32_t integer = 0;
	uint8_t type = get_macsurf_grid(from, &integer);
	css_error err;

	if (from == to) {
		return CSS_OK;
	}

	err = set_macsurf_grid(to, type, integer);
	if (err != CSS_OK) return err;

	macsurf__replace_tracks(to, from->macsurf_grid_tracks);
	return CSS_OK;
}

css_error css__compose_macsurf_grid(const css_computed_style *parent,
		const css_computed_style *child,
		css_computed_style *result)
{
	int32_t integer = 0;
	uint8_t type = get_macsurf_grid(child, &integer);

	return css__copy_macsurf_grid(
			type == CSS_MACSURF_GRID_INHERIT ? parent : child,
			result);
}

uint32_t destroy_macsurf_grid(void *bytecode)
{
	(void)bytecode;
	return 0;
}
