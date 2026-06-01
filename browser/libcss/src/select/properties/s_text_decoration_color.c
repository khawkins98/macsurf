/*
 * This file is part of LibCSS
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2026 MacSurf (fixes357)
 *
 * Cascade text-decoration-color (#44). Not inherited.
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

css_error css__cascade_text_decoration_color(uint32_t opv, css_style *style,
		css_select_state *state)
{
	int32_t status = CSS_TEXT_DECORATION_COLOR_INHERIT;
	css_color color = 0;

	if (hasFlagValue(opv) == false) {
		switch (getValue(opv)) {
		case TEXT_DECORATION_COLOR_CURRENT_COLOR:
			status = CSS_TEXT_DECORATION_COLOR_CURRENT_COLOR;
			break;
		case TEXT_DECORATION_COLOR_SET:
			status = CSS_TEXT_DECORATION_COLOR_COLOR;
			color = *((css_color *) style->bytecode);
			advance_bytecode(style, sizeof(color));
			break;
		}
	}

	if (css__outranks_existing(getOpcode(opv), isImportant(opv), state,
			getFlagValue(opv))) {
		state->computed->i.text_decoration_color_status = status;
		state->computed->i.text_decoration_color = color;
	}

	return CSS_OK;
}

css_error css__set_text_decoration_color_from_hint(const css_hint *hint,
		css_computed_style *style)
{
	style->i.text_decoration_color_status = hint->status;
	style->i.text_decoration_color = hint->data.color;
	return CSS_OK;
}

css_error css__initial_text_decoration_color(css_select_state *state)
{
	state->computed->i.text_decoration_color_status =
			CSS_TEXT_DECORATION_COLOR_CURRENT_COLOR;
	state->computed->i.text_decoration_color = 0;
	return CSS_OK;
}

css_error css__copy_text_decoration_color(
		const css_computed_style *from,
		css_computed_style *to)
{
	if (from == to) return CSS_OK;
	to->i.text_decoration_color_status =
			from->i.text_decoration_color_status;
	to->i.text_decoration_color = from->i.text_decoration_color;
	return CSS_OK;
}

css_error css__compose_text_decoration_color(
		const css_computed_style *parent,
		const css_computed_style *child,
		css_computed_style *result)
{
	int32_t st = child->i.text_decoration_color_status;
	css_color col = child->i.text_decoration_color;
	if (st == CSS_TEXT_DECORATION_COLOR_INHERIT) {
		st = parent->i.text_decoration_color_status;
		col = parent->i.text_decoration_color;
	}
	result->i.text_decoration_color_status = st;
	result->i.text_decoration_color = col;
	return CSS_OK;
}

uint32_t destroy_text_decoration_color(void *bytecode)
{
	(void)bytecode;
	return 0;
}
