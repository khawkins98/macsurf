/*
 * This file is part of LibCSS
 * Licensed under the MIT License,
 *		  http://www.opensource.org/licenses/mit-license.php
 * Copyright 2026 MacSurf (fixes191b)
 *
 * Cascade background-size. Stores an int32_t scalar directly in the
 * inner _i struct (memcmp-safe, self-aligning -- same pattern as
 * fixes151b grid-col-span / fixes152 aspect-ratio).
 *
 * Packed value:
 *   bits 31..16: w_code (int16)
 *   bits 15..0:  h_code (int16)
 *   Codes: 0 = auto, +N = explicit px, -1 = cover, -2 = contain
 *   0 (whole word) = unset / treat as auto auto.
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

css_error css__cascade_background_size(uint32_t opv, css_style *style,
		css_select_state *state)
{
	int32_t packed = 0;
	bool is_set = false;

	if (hasFlagValue(opv) == false) {
		if (getValue(opv) == 0x0080) { /* SET */
			is_set = true;
			packed = *((css_fixed *) style->bytecode);
			advance_bytecode(style, sizeof(css_fixed));
		}
	}

	if (css__outranks_existing(getOpcode(opv), isImportant(opv), state,
			getFlagValue(opv))) {
		state->computed->i.background_size = is_set ? packed : 0;
	}

	return CSS_OK;
}

css_error css__set_background_size_from_hint(const css_hint *hint,
		css_computed_style *style)
{
	(void)hint;
	style->i.background_size = 0;
	return CSS_OK;
}

css_error css__initial_background_size(css_select_state *state)
{
	state->computed->i.background_size = 0;
	return CSS_OK;
}

css_error css__copy_background_size(
		const css_computed_style *from,
		css_computed_style *to)
{
	if (from == to) return CSS_OK;
	to->i.background_size = from->i.background_size;
	return CSS_OK;
}

css_error css__compose_background_size(
		const css_computed_style *parent,
		const css_computed_style *child,
		css_computed_style *result)
{
	/* Not inherited; child wins unless child is unset, in which
	 * case fall back to parent. Same pattern as aspect_ratio. */
	int32_t v = child->i.background_size;
	if (v == 0) v = parent->i.background_size;
	result->i.background_size = v;
	return CSS_OK;
}

uint32_t destroy_background_size(void *bytecode)
{
	(void)bytecode;
	return 0;
}
