/*
 * This file is part of LibCSS.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2026 MacSurf (fixes282, #73)
 *
 * Parse -macsurf-accent-color.
 *
 * The cssh_css.c preprocessor rewrites `accent-color: VALUE` to
 * `-macsurf-accent-color: VALUE` so author CSS reaches this parser
 * without touching libcss's color-property recognition table.
 *
 * Accepts the same color-value grammar as outline-color etc.:
 *   #rrggbb, rgb(...), rgba(...), named colors. Stores as 32-bit
 *   css_color in css_computed_style_i.macsurf_accent_color.
 *
 * `auto` keyword: parsed and stored as unset (0) — form paint then
 * falls back to its built-in highlight color.
 */

#include <assert.h>
#include <string.h>

#include "bytecode/bytecode.h"
#include "bytecode/opcodes.h"
#include "parse/properties/properties.h"
#include "parse/properties/utils.h"

css_error css__parse_macsurf_accent_color(css_language *c,
		const parserutils_vector *vector, int32_t *ctx,
		css_style *result)
{
	int32_t orig_ctx = *ctx;
	css_error error;
	const css_token *token;
	enum flag_value flag_value;
	uint16_t value = 0;
	uint32_t colour = 0;

	token = parserutils_vector_peek(vector, *ctx);
	if (token == NULL) return CSS_INVALID;

	flag_value = get_css_flag_value(c, token);
	if (flag_value != FLAG_VALUE__NONE) {
		parserutils_vector_iterate(vector, ctx);
		return css_stylesheet_style_flag_value(result, flag_value,
				CSS_PROP_MACSURF_ACCENT_COLOR);
	}

	/* `auto` keyword -> SET with colour=0 sentinel (form paint
	 * interprets as "use default highlight"). */
	if (token->type == CSS_TOKEN_IDENT &&
			token->idata != NULL &&
			lwc_string_length(token->idata) == 4) {
		const char *idata = lwc_string_data(token->idata);
		if (idata[0] == 'a' && idata[1] == 'u' &&
				idata[2] == 't' && idata[3] == 'o') {
			parserutils_vector_iterate(vector, ctx);
			error = css__stylesheet_style_appendOPV(result,
					CSS_PROP_MACSURF_ACCENT_COLOR, 0,
					0x0080 /* SET */);
			if (error != CSS_OK) {
				*ctx = orig_ctx;
				return error;
			}
			error = css__stylesheet_style_vappend(result, 1,
					(css_fixed)0);
			if (error != CSS_OK) {
				*ctx = orig_ctx;
				return error;
			}
			return CSS_OK;
		}
	}

	error = css__parse_colour_specifier(c, vector, ctx, &value, &colour);
	if (error != CSS_OK) {
		*ctx = orig_ctx;
		return error;
	}

	error = css__stylesheet_style_appendOPV(result,
			CSS_PROP_MACSURF_ACCENT_COLOR, 0, 0x0080 /* SET */);
	if (error != CSS_OK) {
		*ctx = orig_ctx;
		return error;
	}

	error = css__stylesheet_style_vappend(result, 1, (css_fixed)colour);
	if (error != CSS_OK) {
		*ctx = orig_ctx;
		return error;
	}

	return CSS_OK;
}
