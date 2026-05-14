/*
 * This file is part of LibCSS.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2025 MacSurf (fixes50)
 *
 * Parse -macsurf-text-shadow.
 *
 * Accepted form:
 *   -macsurf-text-shadow: <hoff> <voff> <colour>
 *
 * hoff/voff are integer pixel values (negative allowed).
 * colour is any css colour specifier (hex, named, rgb()...).
 *
 * Storage layout (matches the renderer in plotters.c):
 *   bits 31..24: h-offset (int8 signed)
 *   bits 23..16: v-offset (int8 signed)
 *   bits 15..0:  RGB565 colour
 */

#include <assert.h>
#include <string.h>

#include "bytecode/bytecode.h"
#include "bytecode/opcodes.h"
#include "parse/properties/properties.h"
#include "parse/properties/utils.h"

css_error css__parse_macsurf_text_shadow(css_language *c,
		const parserutils_vector *vector, int32_t *ctx,
		css_style *result)
{
	int32_t orig_ctx = *ctx;
	css_error error;
	const css_token *token;
	css_fixed hoff = 0, voff = 0;
	uint32_t hoff_u = 0, voff_u = 0;
	css_color color = 0;
	uint16_t color_type = 0;
	enum flag_value flag_value;

	token = parserutils_vector_peek(vector, *ctx);
	if (token == NULL) return CSS_INVALID;

	flag_value = get_css_flag_value(c, token);
	if (flag_value != FLAG_VALUE__NONE) {
		parserutils_vector_iterate(vector, ctx);
		return css_stylesheet_style_flag_value(result, flag_value,
				CSS_PROP_MACSURF_TEXT_SHADOW);
	}

	consumeWhitespace(vector, ctx);

	/* h-offset */
	error = css__parse_unit_specifier(c, vector, ctx, UNIT_PX,
			&hoff, &hoff_u);
	if (error != CSS_OK) { *ctx = orig_ctx; return error; }

	consumeWhitespace(vector, ctx);

	/* v-offset */
	error = css__parse_unit_specifier(c, vector, ctx, UNIT_PX,
			&voff, &voff_u);
	if (error != CSS_OK) { *ctx = orig_ctx; return error; }

	consumeWhitespace(vector, ctx);

	/* colour (required) */
	error = css__parse_colour_specifier(c, vector, ctx, &color_type, &color);
	if (error != CSS_OK) { *ctx = orig_ctx; return error; }

	error = css__stylesheet_style_appendOPV(result,
			CSS_PROP_MACSURF_TEXT_SHADOW, 0, 0x0080 /* SET */);
	if (error != CSS_OK) return error;

	return css__stylesheet_style_vappend(result, 3,
			(css_fixed)hoff, (css_fixed)voff, (css_fixed)color);
}
