/*
 * This file is part of LibCSS
 * Licensed under the MIT License,
 *		  http://www.opensource.org/licenses/mit-license.php
 * Copyright 2026 MacSurf (fixes71)
 */

#include "bytecode/bytecode.h"
#include "bytecode/opcodes.h"
#include "select/propset.h"
#include "select/propget.h"
#include "utils/utils.h"

#include "select/properties/properties.h"
#include "select/properties/helpers.h"

/* fixes71 -- pack rotation + translate into one int32_t storage slot.
 *   bits 31..16 rotation angle Q10.6 deg (signed, treated mod 360)
 *   bits 15..8  translate-x int8 px (-128..127)
 *   bits 7..0   translate-y int8 px (-128..127)
 *
 * Rotation conversion: bytecode rotation arrives as Q22.10 degrees
 * (the libcss native unit format). Shift right by 4 to land in Q10.6
 * (one degree = 64 ticks).  Wrap into ±32768 range so the int16 cast
 * doesn't overflow.
 *
 * Translation arrives as Q22.10 px. Shift right by 10 to land in
 * integer px; clamp to int8 range. */
static int32_t macsurf_transform_pack(css_fixed rotation,
		css_fixed tx, css_fixed ty)
{
	int32_t rot_q106 = (int32_t)(rotation >> 4);
	int32_t tx_px = (int32_t)(tx >> 10);
	int32_t ty_px = (int32_t)(ty >> 10);
	int32_t mod_full = 360 * 64;  /* 360deg in Q10.6 */
	uint32_t out;

	/* Wrap rotation into a single 360° turn so the int16 slot is
	 * always in range regardless of how many turns the author asked
	 * for. */
	while (rot_q106 < -mod_full) rot_q106 += mod_full;
	while (rot_q106 >= mod_full) rot_q106 -= mod_full;

	if (tx_px > 127) tx_px = 127;
	if (tx_px < -128) tx_px = -128;
	if (ty_px > 127) ty_px = 127;
	if (ty_px < -128) ty_px = -128;

	out = (((uint32_t)((uint16_t)((int16_t)rot_q106))) << 16) |
	      (((uint32_t)((uint8_t)tx_px))                  << 8)  |
	       ((uint32_t)((uint8_t)ty_px));

	return (int32_t)out;
}

css_error css__cascade_macsurf_transform(uint32_t opv, css_style *style,
		css_select_state *state)
{
	uint16_t value = CSS_MACSURF_TRANSFORM_INHERIT;
	css_fixed rotation = 0;
	css_fixed tx = 0;
	css_fixed ty = 0;
	int32_t packed = 0;

	if (hasFlagValue(opv) == false) {
		switch (getValue(opv)) {
		case 0x0000: /* NONE */
			value = CSS_MACSURF_TRANSFORM_NONE;
			break;
		case 0x0080: /* SET */
			value = CSS_MACSURF_TRANSFORM_SET;
			rotation = *((css_fixed *) style->bytecode);
			advance_bytecode(style, sizeof(css_fixed));
			tx = *((css_fixed *) style->bytecode);
			advance_bytecode(style, sizeof(css_fixed));
			ty = *((css_fixed *) style->bytecode);
			advance_bytecode(style, sizeof(css_fixed));
			packed = macsurf_transform_pack(rotation, tx, ty);
			break;
		}
	}

	if (css__outranks_existing(getOpcode(opv), isImportant(opv), state,
			getFlagValue(opv))) {
		return set_macsurf_transform(state->computed, value, packed);
	}

	return CSS_OK;
}

css_error css__set_macsurf_transform_from_hint(const css_hint *hint,
		css_computed_style *style)
{
	return set_macsurf_transform(style, hint->status,
			hint->data.integer);
}

css_error css__initial_macsurf_transform(css_select_state *state)
{
	return set_macsurf_transform(state->computed,
			CSS_MACSURF_TRANSFORM_NONE, 0);
}

css_error css__copy_macsurf_transform(
		const css_computed_style *from,
		css_computed_style *to)
{
	int32_t integer = 0;
	uint8_t type = get_macsurf_transform(from, &integer);

	if (from == to) {
		return CSS_OK;
	}

	return set_macsurf_transform(to, type, integer);
}

css_error css__compose_macsurf_transform(const css_computed_style *parent,
		const css_computed_style *child,
		css_computed_style *result)
{
	int32_t integer = 0;
	uint8_t type = get_macsurf_transform(child, &integer);

	return css__copy_macsurf_transform(
			type == CSS_MACSURF_TRANSFORM_INHERIT ? parent : child,
			result);
}

uint32_t destroy_macsurf_transform(void *bytecode)
{
	(void)bytecode;
	return 0;
}
