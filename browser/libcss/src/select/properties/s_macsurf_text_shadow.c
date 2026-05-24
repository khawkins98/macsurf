/*
 * This file is part of LibCSS
 * Licensed under the MIT License,
 *		  http://www.opensource.org/licenses/mit-license.php
 * Copyright 2025 MacSurf (fixes50)
 */

#include "bytecode/bytecode.h"
#include "bytecode/opcodes.h"
#include "select/propset.h"
#include "select/propget.h"
#include "utils/utils.h"

#include "select/properties/properties.h"
#include "select/properties/helpers.h"

/* fixes50/200 -- pack h-offset, v-offset, blur, and RGB565 colour into the
 * single int32_t storage slot reserved for macsurf_text_shadow.
 *   bits 31..26 h-offset px (6-bit signed, -32..31)
 *   bits 25..20 v-offset px (6-bit signed, -32..31)
 *   bits 19..16 blur radius px (4-bit unsigned, 0..15)
 *   bits 15..0  RGB565 colour
 */
static int32_t macsurf_text_shadow_pack(css_fixed h, css_fixed v,
		css_fixed blur, css_color color)
{
	int32_t h_px = (int32_t)(h >> 10);
	int32_t v_px = (int32_t)(v >> 10);
	int32_t b_px = (int32_t)(blur >> 10);
	uint8_t r = (uint8_t)((color >> 16) & 0xff);
	uint8_t g = (uint8_t)((color >>  8) & 0xff);
	uint8_t b = (uint8_t)((color >>  0) & 0xff);
	uint16_t rgb565;
	uint32_t out;

	if (h_px > 31) h_px = 31;
	if (h_px < -32) h_px = -32;
	if (v_px > 31) v_px = 31;
	if (v_px < -32) v_px = -32;
	if (b_px > 15) b_px = 15;
	if (b_px < 0) b_px = 0;

	rgb565 = (uint16_t)((((uint32_t)r >> 3) << 11) |
			(((uint32_t)g >> 2) <<  5) |
			 ((uint32_t)b >> 3));

	out = (((uint32_t)((uint8_t)h_px & 0x3f)) << 26) |
	      (((uint32_t)((uint8_t)v_px & 0x3f)) << 20) |
	      (((uint32_t)((uint8_t)b_px & 0x0f)) << 16) |
	       (uint32_t)rgb565;

	return (int32_t)out;
}

css_error css__cascade_macsurf_text_shadow(uint32_t opv, css_style *style,
                css_select_state *state)
{
        uint16_t value = CSS_MACSURF_TEXT_SHADOW_INHERIT;
        css_fixed h = 0, v = 0, blur = 0;
        css_color color = 0;
        int32_t packed = 0;

        if (hasFlagValue(opv) == false) {
                switch (getValue(opv)) {
                case 0x0000: /* NONE */
                        value = CSS_MACSURF_TEXT_SHADOW_NONE;
                        break;
                case 0x0080: /* SET */
                        value = CSS_MACSURF_TEXT_SHADOW_SET;
                        h = *((css_fixed *) style->bytecode);
                        advance_bytecode(style, sizeof(css_fixed));
                        v = *((css_fixed *) style->bytecode);
                        advance_bytecode(style, sizeof(css_fixed));
                        blur = *((css_fixed *) style->bytecode);
                        advance_bytecode(style, sizeof(css_fixed));
                        color = *((css_color *) style->bytecode);
                        advance_bytecode(style, sizeof(css_color));
                        packed = macsurf_text_shadow_pack(h, v, blur, color);
                        break;
                }
        }
	if (css__outranks_existing(getOpcode(opv), isImportant(opv), state,
			getFlagValue(opv))) {
		return set_macsurf_text_shadow(state->computed, value, packed);
	}

	return CSS_OK;
}

css_error css__set_macsurf_text_shadow_from_hint(const css_hint *hint,
		css_computed_style *style)
{
	return set_macsurf_text_shadow(style, hint->status,
			hint->data.integer);
}

css_error css__initial_macsurf_text_shadow(css_select_state *state)
{
	return set_macsurf_text_shadow(state->computed,
			CSS_MACSURF_TEXT_SHADOW_NONE, 0);
}

css_error css__copy_macsurf_text_shadow(
		const css_computed_style *from,
		css_computed_style *to)
{
	int32_t integer = 0;
	uint8_t type = get_macsurf_text_shadow(from, &integer);

	if (from == to) {
		return CSS_OK;
	}

	return set_macsurf_text_shadow(to, type, integer);
}

css_error css__compose_macsurf_text_shadow(const css_computed_style *parent,
		const css_computed_style *child,
		css_computed_style *result)
{
	int32_t integer = 0;
	uint8_t type = get_macsurf_text_shadow(child, &integer);

	return css__copy_macsurf_text_shadow(
			type == CSS_MACSURF_TEXT_SHADOW_INHERIT ? parent : child,
			result);
}

uint32_t destroy_macsurf_text_shadow(void *bytecode)
{
	(void)bytecode;
	return 0;
}
