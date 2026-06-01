/*
 * This file is part of LibCSS
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2026 MacSurf (fixes357)
 *
 * Cascade text-decoration-thickness (#44). Not inherited.
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

css_error css__cascade_text_decoration_thickness(uint32_t opv, css_style *style,
		css_select_state *state)
{
	int32_t n = 0; /* 0 = auto / from-font */

	if (hasFlagValue(opv) == false) {
		if (getValue(opv) == TEXT_DECORATION_THICKNESS_SET) {
			n = *((css_fixed *) style->bytecode);
			advance_bytecode(style, sizeof(css_fixed));
		}
	}

	if (css__outranks_existing(getOpcode(opv), isImportant(opv), state,
			getFlagValue(opv))) {
		state->computed->i.text_decoration_thickness = n;
	}

	return CSS_OK;
}

css_error css__set_text_decoration_thickness_from_hint(const css_hint *hint,
		css_computed_style *style)
{
	(void)hint;
	style->i.text_decoration_thickness = 0;
	return CSS_OK;
}

css_error css__initial_text_decoration_thickness(css_select_state *state)
{
	state->computed->i.text_decoration_thickness = 0;
	return CSS_OK;
}

css_error css__copy_text_decoration_thickness(
		const css_computed_style *from,
		css_computed_style *to)
{
	if (from == to) return CSS_OK;
	to->i.text_decoration_thickness = from->i.text_decoration_thickness;
	return CSS_OK;
}

css_error css__compose_text_decoration_thickness(
		const css_computed_style *parent,
		const css_computed_style *child,
		css_computed_style *result)
{
	int32_t v = child->i.text_decoration_thickness;
	(void)parent;
	/* Not inherited; child always wins. */
	result->i.text_decoration_thickness = v;
	return CSS_OK;
}

uint32_t destroy_text_decoration_thickness(void *bytecode)
{
	(void)bytecode;
	return 0;
}
