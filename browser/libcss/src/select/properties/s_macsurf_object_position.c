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
	int32_t numeric_xy = 0;
	bool is_numeric = false;

	if (hasFlagValue(opv) == false) {
		value = getValue(opv);
		/* fixes201: SET (0x0080) is the marker for the numeric
		 * (percent / px) form. Pull the packed int32 off the
		 * bytecode stream and remember to write it to the
		 * computed-style's macsurf_object_position_xy field. The
		 * keyword field gets the default centre value so the
		 * keyword-fallback path in the consumer doesn't paint
		 * anywhere unexpected. */
		if (value == 0x0080) {
			is_numeric = true;
			numeric_xy = *((css_fixed *) style->bytecode);
			advance_bytecode(style, sizeof(css_fixed));
			value = macsurf_object_position_initial_value();
		}
	}

	if (css__outranks_existing(getOpcode(opv), isImportant(opv), state,
			getFlagValue(opv))) {
		css_error err = set_macsurf_object_position(state->computed,
				(uint8_t)value);
		if (err != CSS_OK) return err;
		/* Always write the xy field. When the cascade emitted a
		 * keyword (is_numeric == false), the field is set to 0
		 * which means "unset; use the keyword". When numeric,
		 * the packed int32 carries the actual values. */
		state->computed->i.macsurf_object_position_xy =
				is_numeric ? numeric_xy : 0;
	}

	return CSS_OK;
}

css_error css__set_macsurf_object_position_from_hint(const css_hint *hint,
		css_computed_style *style)
{
	style->i.macsurf_object_position_xy = 0;
	return set_macsurf_object_position(style, hint->status);
}

css_error css__initial_macsurf_object_position(css_select_state *state)
{
	state->computed->i.macsurf_object_position_xy = 0;
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

	to->i.macsurf_object_position_xy =
			from->i.macsurf_object_position_xy;
	return set_macsurf_object_position(to,
			get_macsurf_object_position(from));
}

css_error css__compose_macsurf_object_position(
		const css_computed_style *parent,
		const css_computed_style *child,
		css_computed_style *result)
{
	uint8_t type = get_macsurf_object_position(child);
	int32_t child_xy = child->i.macsurf_object_position_xy;

	/* Compose: when the child has either an explicit keyword OR a
	 * numeric value, the child wins. INHERIT keyword + zero xy
	 * means "no child declaration"; defer to parent. */
	if (type == CSS_MACSURF_OBJECT_POSITION_INHERIT && child_xy == 0) {
		return css__copy_macsurf_object_position(parent, result);
	}
	return css__copy_macsurf_object_position(child, result);
}

uint32_t destroy_macsurf_object_position(void *bytecode)
{
	(void)bytecode;
	return 0;
}
