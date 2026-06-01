/*
 * This file is part of LibCSS
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2026 MacSurf (fixes353)
 *
 * Cascade caret-color (#73). Inherited. Self-aligning int32_t status
 * + css_color storage at end of _i (per fixes151b padding lesson).
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

css_error css__cascade_caret_color(uint32_t opv, css_style *style,
		css_select_state *state)
{
	int32_t status = 0;
	css_color color = 0;

	if (hasFlagValue(opv) == false) {
		switch (getValue(opv)) {
		case CARET_COLOR_AUTO:
			status = CSS_CARET_COLOR_AUTO;
			break;
		case CARET_COLOR_CURRENT_COLOR:
			status = CSS_CARET_COLOR_CURRENT_COLOR;
			break;
		case CARET_COLOR_SET:
			status = CSS_CARET_COLOR_COLOR;
			color = *((css_color *) style->bytecode);
			advance_bytecode(style, sizeof(color));
			break;
		}
	}

	if (css__outranks_existing(getOpcode(opv), isImportant(opv), state,
			getFlagValue(opv))) {
		state->computed->i.caret_color_status = status;
		state->computed->i.caret_color = color;
	}

	return CSS_OK;
}

css_error css__set_caret_color_from_hint(const css_hint *hint,
		css_computed_style *style)
{
	style->i.caret_color_status = hint->status;
	style->i.caret_color = hint->data.color;
	return CSS_OK;
}

css_error css__initial_caret_color(css_select_state *state)
{
	state->computed->i.caret_color_status = CSS_CARET_COLOR_AUTO;
	state->computed->i.caret_color = 0;
	return CSS_OK;
}

css_error css__copy_caret_color(
		const css_computed_style *from,
		css_computed_style *to)
{
	if (from == to) return CSS_OK;
	to->i.caret_color_status = from->i.caret_color_status;
	to->i.caret_color = from->i.caret_color;
	return CSS_OK;
}

css_error css__compose_caret_color(
		const css_computed_style *parent,
		const css_computed_style *child,
		css_computed_style *result)
{
	int32_t st = child->i.caret_color_status;
	css_color col = child->i.caret_color;
	if (st == CSS_CARET_COLOR_INHERIT) {
		st = parent->i.caret_color_status;
		col = parent->i.caret_color;
	}
	result->i.caret_color_status = st;
	result->i.caret_color = col;
	return CSS_OK;
}

uint32_t destroy_caret_color(void *bytecode)
{
	(void)bytecode;
	return 0;
}
