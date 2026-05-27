/*
 * This file is part of LibCSS
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2026 MacSurf (fixes284, #73)
 *
 * Cascade -macsurf-caret-color. Stores a packed css_color directly in
 * css_computed_style_i.macsurf_caret_color (int32). 0 means unset
 * (form paint falls back to its built-in highlight color).
 *
 * Per-element, NOT inherited. (Spec actually says caret-color IS
 * inherited; for V1 we keep it per-element to match the typical use
 * pattern `input { caret-color: red }` and avoid surprises from
 * cascade inheritance to non-form descendants.)
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

css_error css__cascade_macsurf_caret_color(uint32_t opv,
		css_style *style, css_select_state *state)
{
	int32_t col = 0;
	bool is_set = false;

	if (hasFlagValue(opv) == false) {
		if (getValue(opv) == 0x0080) { /* SET */
			is_set = true;
			col = *((css_fixed *) style->bytecode);
			advance_bytecode(style, sizeof(css_fixed));
		}
	}

	if (css__outranks_existing(getOpcode(opv), isImportant(opv), state,
			getFlagValue(opv))) {
		state->computed->i.macsurf_caret_color = is_set ? col : 0;
	}

	return CSS_OK;
}

css_error css__set_macsurf_caret_color_from_hint(const css_hint *hint,
		css_computed_style *style)
{
	(void)hint;
	style->i.macsurf_caret_color = 0;
	return CSS_OK;
}

css_error css__initial_macsurf_caret_color(css_select_state *state)
{
	state->computed->i.macsurf_caret_color = 0;
	return CSS_OK;
}

css_error css__copy_macsurf_caret_color(
		const css_computed_style *from,
		css_computed_style *to)
{
	if (from == to) return CSS_OK;
	to->i.macsurf_caret_color = from->i.macsurf_caret_color;
	return CSS_OK;
}

css_error css__compose_macsurf_caret_color(
		const css_computed_style *parent,
		const css_computed_style *child,
		css_computed_style *result)
{
	(void)parent;
	result->i.macsurf_caret_color = child->i.macsurf_caret_color;
	return CSS_OK;
}

uint32_t destroy_macsurf_caret_color(void *bytecode)
{
	(void)bytecode;
	return 0;
}
