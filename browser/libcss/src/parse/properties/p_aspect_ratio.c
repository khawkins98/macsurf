/*
 * This file is part of LibCSS.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2026 MacSurf (fixes152)
 *
 * Parse aspect-ratio.
 *
 * Accepted forms:
 *   aspect-ratio: <number>          -> width:height = <number>:1
 *   aspect-ratio: <number> / <number>  -> width:height = num:denom
 *
 * Deferred:
 *   aspect-ratio: auto              -> preserve natural ratio for
 *                                       replaced elements
 *   aspect-ratio: auto <ratio>      -> auto with fallback
 *
 * Storage in css_computed_style_i.aspect_ratio (int32_t scalar, self-
 * aligning to avoid the padding trap from fixes151b):
 *   bits 31..16: width numerator   (1..65535)
 *   bits 15..0:  height denominator (1..65535)
 *   0 (whole word) = unset (treat as no aspect-ratio constraint).
 *
 * Single-number form `1.5` is scaled by 1000 -> num=1500, denom=1000.
 * Ratio form `16 / 9` stores num=16, denom=9 directly. Layout uses
 * straight integer math (h = w * denom / num, w = h * num / denom),
 * staying away from the CW8 PPC long-long miscompile entirely.
 */

#include <assert.h>
#include <string.h>

#include "bytecode/bytecode.h"
#include "bytecode/opcodes.h"
#include "parse/properties/properties.h"
#include "parse/properties/utils.h"

static int32_t aspect_ratio__read_number(const css_token *t)
{
	size_t consumed = 0;
	css_fixed num;
	int32_t v;
	if (t == NULL || t->idata == NULL) return 0;
	num = css__number_from_lwc_string(t->idata, false, &consumed);
	/* css_fixed is Q22.10. Scale by 1000 / 1024 to convert to
	 * thousandths -- 3 decimal digits of precision, enough for
	 * typical aspect-ratio values like 1.7777 (16:9). Done in
	 * int32 to dodge the CW8 PPC long-long miscompile (fixes113);
	 * num is clamped well below INT32_MAX/1000 by the 65535
	 * post-shift cap, so num * 1000 cannot overflow for valid
	 * inputs. */
	v = (int32_t)((num * 1000) >> 10);
	if (v < 1) v = 1;
	if (v > 65535) v = 65535;
	return v;
}

css_error css__parse_aspect_ratio(css_language *c,
		const parserutils_vector *vector, int32_t *ctx,
		css_style *result)
{
	int32_t orig_ctx = *ctx;
	css_error error;
	const css_token *token;
	const css_token *num_tok;
	enum flag_value flag_value;
	int32_t num = 0;
	int32_t denom = 0;
	int32_t packed;

	token = parserutils_vector_peek(vector, *ctx);
	if (token == NULL) return CSS_INVALID;

	flag_value = get_css_flag_value(c, token);
	if (flag_value != FLAG_VALUE__NONE) {
		parserutils_vector_iterate(vector, ctx);
		return css_stylesheet_style_flag_value(result, flag_value,
				CSS_PROP_ASPECT_RATIO);
	}

	consumeWhitespace(vector, ctx);

	token = parserutils_vector_peek(vector, *ctx);
	if (token == NULL || token->type != CSS_TOKEN_NUMBER ||
			token->idata == NULL) {
		*ctx = orig_ctx;
		return CSS_INVALID;
	}
	num_tok = token;
	num = aspect_ratio__read_number(num_tok);
	parserutils_vector_iterate(vector, ctx);

	consumeWhitespace(vector, ctx);

	token = parserutils_vector_peek(vector, *ctx);
	if (token != NULL && token->type == CSS_TOKEN_CHAR &&
			token->idata != NULL &&
			lwc_string_length(token->idata) == 1 &&
			lwc_string_data(token->idata)[0] == '/') {
		/* Explicit numerator / denominator form. */
		parserutils_vector_iterate(vector, ctx);
		consumeWhitespace(vector, ctx);
		token = parserutils_vector_peek(vector, *ctx);
		if (token == NULL || token->type != CSS_TOKEN_NUMBER ||
				token->idata == NULL) {
			*ctx = orig_ctx;
			return CSS_INVALID;
		}
		denom = aspect_ratio__read_number(token);
		parserutils_vector_iterate(vector, ctx);
	} else {
		/* Single-number form: ratio is <num>:1. The scaled `num`
		 * already encodes the value times 1000; use 1000 as the
		 * implicit denominator. */
		denom = 1000;
	}

	if (num < 1) num = 1;
	if (denom < 1) denom = 1;
	if (num > 65535) num = 65535;
	if (denom > 65535) denom = 65535;

	packed = ((int32_t)(((uint32_t)num & 0xFFFF) << 16)) |
			(int32_t)((uint32_t)denom & 0xFFFF);

	error = css__stylesheet_style_appendOPV(result,
			CSS_PROP_ASPECT_RATIO, 0, 0x0080 /* SET */);
	if (error != CSS_OK) {
		*ctx = orig_ctx;
		return error;
	}

	error = css__stylesheet_style_vappend(result, 1, (css_fixed)packed);
	if (error != CSS_OK) {
		*ctx = orig_ctx;
		return error;
	}

	return CSS_OK;
}
