/*
 * This file is part of LibCSS
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2026 MacSurf (fixes357)
 *
 * Cascade text-decoration-style (#44). Not inherited.
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

css_error css__cascade_text_decoration_style(uint32_t opv, css_style *style,
		css_select_state *state)
{
	int32_t v = CSS_TEXT_DECORATION_STYLE_INHERIT;
	(void)style;

	if (hasFlagValue(opv) == false) {
		switch (getValue(opv)) {
		case TEXT_DECORATION_STYLE_SOLID:
			v = CSS_TEXT_DECORATION_STYLE_SOLID;
			break;
		case TEXT_DECORATION_STYLE_DOUBLE:
			v = CSS_TEXT_DECORATION_STYLE_DOUBLE;
			break;
		case TEXT_DECORATION_STYLE_DOTTED:
			v = CSS_TEXT_DECORATION_STYLE_DOTTED;
			break;
		case TEXT_DECORATION_STYLE_DASHED:
			v = CSS_TEXT_DECORATION_STYLE_DASHED;
			break;
		case TEXT_DECORATION_STYLE_WAVY:
			v = CSS_TEXT_DECORATION_STYLE_WAVY;
			break;
		}
	}

	if (css__outranks_existing(getOpcode(opv), isImportant(opv), state,
			getFlagValue(opv))) {
		state->computed->i.text_decoration_style = v;
	}

	return CSS_OK;
}

css_error css__set_text_decoration_style_from_hint(const css_hint *hint,
		css_computed_style *style)
{
	style->i.text_decoration_style = hint->status;
	return CSS_OK;
}

css_error css__initial_text_decoration_style(css_select_state *state)
{
	state->computed->i.text_decoration_style =
			CSS_TEXT_DECORATION_STYLE_SOLID;
	return CSS_OK;
}

css_error css__copy_text_decoration_style(
		const css_computed_style *from,
		css_computed_style *to)
{
	if (from == to) return CSS_OK;
	to->i.text_decoration_style = from->i.text_decoration_style;
	return CSS_OK;
}

css_error css__compose_text_decoration_style(
		const css_computed_style *parent,
		const css_computed_style *child,
		css_computed_style *result)
{
	int32_t v = child->i.text_decoration_style;
	if (v == CSS_TEXT_DECORATION_STYLE_INHERIT) {
		v = parent->i.text_decoration_style;
	}
	result->i.text_decoration_style = v;
	return CSS_OK;
}

uint32_t destroy_text_decoration_style(void *bytecode)
{
	(void)bytecode;
	return 0;
}
