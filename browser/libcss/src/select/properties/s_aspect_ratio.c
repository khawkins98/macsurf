/*
 * This file is part of LibCSS
 * Licensed under the MIT License,
 *		  http://www.opensource.org/licenses/mit-license.php
 * Copyright 2026 MacSurf (fixes152)
 *
 * Cascade aspect-ratio. Stores an int32_t scalar directly in the
 * inner _i struct (memcmp-safe, self-aligning -- same pattern as
 * fixes151b's grid-col-span which dodged the padding trap).
 *
 * Packed value:
 *   bits 31..16: numerator   (1..65535)
 *   bits 15..0:  denominator (1..65535)
 *   0 (whole word) = unset / no aspect-ratio constraint.
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

css_error css__cascade_aspect_ratio(uint32_t opv, css_style *style,
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
		state->computed->i.aspect_ratio = is_set ? packed : 0;
	}

	return CSS_OK;
}

css_error css__set_aspect_ratio_from_hint(const css_hint *hint,
		css_computed_style *style)
{
	(void)hint;
	style->i.aspect_ratio = 0;
	return CSS_OK;
}

css_error css__initial_aspect_ratio(css_select_state *state)
{
	state->computed->i.aspect_ratio = 0;
	return CSS_OK;
}

css_error css__copy_aspect_ratio(
		const css_computed_style *from,
		css_computed_style *to)
{
	if (from == to) return CSS_OK;
	to->i.aspect_ratio = from->i.aspect_ratio;
	return CSS_OK;
}

css_error css__compose_aspect_ratio(
		const css_computed_style *parent,
		const css_computed_style *child,
		css_computed_style *result)
{
	/* Not inherited; child wins unless child is unset, in which
	 * case fall back to parent. Same pattern as col-span. */
	int32_t v = child->i.aspect_ratio;
	if (v == 0) v = parent->i.aspect_ratio;
	result->i.aspect_ratio = v;
	return CSS_OK;
}

uint32_t destroy_aspect_ratio(void *bytecode)
{
	(void)bytecode;
	return 0;
}
