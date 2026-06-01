/*
 * This file is part of LibCSS
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2026 MacSurf (fixes353)
 *
 * Cascade accent-color (#73). Inherited. Self-aligning int32_t
 * status + css_color storage at the end of _i (per fixes151b padding
 * lesson).
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

css_error css__cascade_accent_color(uint32_t opv, css_style *style,
		css_select_state *state)
{
	int32_t status = 0;
	css_color color = 0;

	if (hasFlagValue(opv) == false) {
		switch (getValue(opv)) {
		case ACCENT_COLOR_AUTO:
			status = CSS_ACCENT_COLOR_AUTO;
			break;
		case ACCENT_COLOR_CURRENT_COLOR:
			status = CSS_ACCENT_COLOR_CURRENT_COLOR;
			break;
		case ACCENT_COLOR_SET:
			status = CSS_ACCENT_COLOR_COLOR;
			color = *((css_color *) style->bytecode);
			advance_bytecode(style, sizeof(color));
			break;
		}
	}

	if (css__outranks_existing(getOpcode(opv), isImportant(opv), state,
			getFlagValue(opv))) {
		state->computed->i.accent_color_status = status;
		state->computed->i.accent_color = color;
	}

	return CSS_OK;
}

css_error css__set_accent_color_from_hint(const css_hint *hint,
		css_computed_style *style)
{
	style->i.accent_color_status = hint->status;
	style->i.accent_color = hint->data.color;
	return CSS_OK;
}

css_error css__initial_accent_color(css_select_state *state)
{
	state->computed->i.accent_color_status = CSS_ACCENT_COLOR_AUTO;
	state->computed->i.accent_color = 0;
	return CSS_OK;
}

css_error css__copy_accent_color(
		const css_computed_style *from,
		css_computed_style *to)
{
	if (from == to) return CSS_OK;
	to->i.accent_color_status = from->i.accent_color_status;
	to->i.accent_color = from->i.accent_color;
	return CSS_OK;
}

css_error css__compose_accent_color(
		const css_computed_style *parent,
		const css_computed_style *child,
		css_computed_style *result)
{
	int32_t st = child->i.accent_color_status;
	css_color col = child->i.accent_color;
	if (st == CSS_ACCENT_COLOR_INHERIT) {
		st = parent->i.accent_color_status;
		col = parent->i.accent_color;
	}
	result->i.accent_color_status = st;
	result->i.accent_color = col;
	return CSS_OK;
}

uint32_t destroy_accent_color(void *bytecode)
{
	(void)bytecode;
	return 0;
}
