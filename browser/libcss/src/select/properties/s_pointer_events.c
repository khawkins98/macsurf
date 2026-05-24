/*
 * This file is part of LibCSS.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2026 Gemini CLI
 */

#include "bytecode/bytecode.h"
#include "bytecode/opcodes.h"
#include "select/propset.h"
#include "select/propget.h"
#include "utils/utils.h"

#include "select/properties/properties.h"
#include "select/properties/helpers.h"

css_error css__cascade_pointer_events(uint32_t opv, css_style *style, 
		css_select_state *state)
{
	uint16_t value = CSS_POINTER_EVENTS_INHERIT;

	if (hasFlagValue(opv) == false) {
		switch (getValue(opv)) {
		case CSS_POINTER_EVENTS_AUTO:
			value = CSS_POINTER_EVENTS_AUTO;
			break;
		case CSS_POINTER_EVENTS_NONE:
			value = CSS_POINTER_EVENTS_NONE;
			break;
		}
	}

	if (css__outranks_existing(getOpcode(opv), isImportant(opv), state,
			getFlagValue(opv))) {
		return set_pointer_events(state->computed, value);
	}

	return CSS_OK;
}

css_error css__set_pointer_events_from_hint(const css_hint *hint,
		css_computed_style *style)
{
	return set_pointer_events(style, hint->status);
}

css_error css__initial_pointer_events(css_select_state *state)
{
	return set_pointer_events(state->computed, CSS_POINTER_EVENTS_AUTO);
}

css_error css__copy_pointer_events(
		const css_computed_style *from,
		css_computed_style *to)
{
	uint8_t type = get_pointer_events(from);

	if (from == to)
		return CSS_OK;

	return set_pointer_events(to, type);
}

css_error css__compose_pointer_events(const css_computed_style *parent,
		const css_computed_style *child,
		css_computed_style *result)
{
	uint8_t type = get_pointer_events(child);

	if (type == CSS_POINTER_EVENTS_INHERIT) {
		type = get_pointer_events(parent);
	}

	return set_pointer_events(result, type);
}

uint32_t destroy_pointer_events(void *bytecode)
{
	return 0;
}
