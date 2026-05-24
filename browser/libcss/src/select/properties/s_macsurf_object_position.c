/*
 * This file is part of LibCSS
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2026 MacSurf
 */

#include "bytecode/bytecode.h"
#include "bytecode/opcodes.h"
#include "select/propset.h"
#include "select/propget.h"
#include "utils/utils.h"

#include "select/properties/properties.h"
#include "select/properties/helpers.h"

static uint8_t
macsurf_object_position_initial_value(void)
{
	return (uint8_t)((CSS_MACSURF_OBJECT_POSITION_CENTER << 2) |
			CSS_MACSURF_OBJECT_POSITION_CENTER);
}

css_error css__cascade_macsurf_object_position(uint32_t opv, css_style *style,
		css_select_state *state)
{
	uint16_t value = CSS_MACSURF_OBJECT_POSITION_INHERIT;

	UNUSED(style);

	if (hasFlagValue(opv) == false) {
		value = getValue(opv);
	}

	if (css__outranks_existing(getOpcode(opv), isImportant(opv), state,
			getFlagValue(opv))) {
		return set_macsurf_object_position(state->computed,
				(uint8_t)value);
	}

	return CSS_OK;
}

css_error css__set_macsurf_object_position_from_hint(const css_hint *hint,
		css_computed_style *style)
{
	return set_macsurf_object_position(style, hint->status);
}

css_error css__initial_macsurf_object_position(css_select_state *state)
{
	return set_macsurf_object_position(state->computed,
			macsurf_object_position_initial_value());
}

css_error css__copy_macsurf_object_position(
		const css_computed_style *from,
		css_computed_style *to)
{
	if (from == to) {
		return CSS_OK;
	}

	return set_macsurf_object_position(to,
			get_macsurf_object_position(from));
}

css_error css__compose_macsurf_object_position(
		const css_computed_style *parent,
		const css_computed_style *child,
		css_computed_style *result)
{
	uint8_t type = get_macsurf_object_position(child);

	return css__copy_macsurf_object_position(
			type == CSS_MACSURF_OBJECT_POSITION_INHERIT ?
			parent : child,
			result);
}

uint32_t destroy_macsurf_object_position(void *bytecode)
{
	(void)bytecode;
	return 0;
}
