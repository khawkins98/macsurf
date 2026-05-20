/*
 * This file is part of LibCSS.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2026 MacSurf (fixes151)
 *
 * Parse -macsurf-grid-col-span.
 *
 * The cssh_css.c preprocessor rewrites the standard CSS grid placement
 * shorthands `grid-column: span N`, `grid-column: A / B`, and
 * `grid-column: 1 / -1` into `-macsurf-grid-col-span: N` integer
 * declarations. By the time this parser runs the value is a single
 * positive integer in 1..255 (255 is the "fill the rest of the row"
 * sentinel for `1 / -1`-style declarations).
 *
 * Bytecode payload after appendOPV(SET): one int32 holding the span
 * count. Storage in css_computed_style_i.macsurf_grid_col_span
 * (uint8_t scalar; 0 means unset, treat as 1 in layout).
 */

#include <assert.h>
#include <string.h>

#include "bytecode/bytecode.h"
#include "bytecode/opcodes.h"
#include "parse/properties/properties.h"
#include "parse/properties/utils.h"

css_error css__parse_macsurf_grid_col_span(css_language *c,
		const parserutils_vector *vector, int32_t *ctx,
		css_style *result)
{
	int32_t orig_ctx = *ctx;
	css_error error;
	const css_token *token;
	enum flag_value flag_value;
	int32_t span = 0;

	token = parserutils_vector_peek(vector, *ctx);
	if (token == NULL) return CSS_INVALID;

	flag_value = get_css_flag_value(c, token);
	if (flag_value != FLAG_VALUE__NONE) {
		parserutils_vector_iterate(vector, ctx);
		return css_stylesheet_style_flag_value(result, flag_value,
				CSS_PROP_MACSURF_GRID_COL_SPAN);
	}

	consumeWhitespace(vector, ctx);

	token = parserutils_vector_peek(vector, *ctx);
	if (token == NULL || token->type != CSS_TOKEN_NUMBER ||
			token->idata == NULL) {
		*ctx = orig_ctx;
		return CSS_INVALID;
	}

	{
		size_t consumed = 0;
		css_fixed num = css__number_from_lwc_string(
				token->idata, true, &consumed);
		int32_t val = (int32_t)(num >> 10);
		if (val < 1) val = 1;
		if (val > 255) val = 255;
		span = val;
		parserutils_vector_iterate(vector, ctx);
	}

	error = css__stylesheet_style_appendOPV(result,
			CSS_PROP_MACSURF_GRID_COL_SPAN, 0, 0x0080 /* SET */);
	if (error != CSS_OK) {
		*ctx = orig_ctx;
		return error;
	}

	error = css__stylesheet_style_vappend(result, 1, (css_fixed)span);
	if (error != CSS_OK) {
		*ctx = orig_ctx;
		return error;
	}

	return CSS_OK;
}
