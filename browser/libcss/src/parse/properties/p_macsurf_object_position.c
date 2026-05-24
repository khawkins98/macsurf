/*
 * This file is part of LibCSS.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2026 MacSurf
 *
 * Parse `-macsurf-object-position`.
 *
 * V1 grammar is intentionally narrow: one or two keywords from
 *   left | center | right | top | bottom
 * with the usual CSS defaults:
 *   left         -> left center
 *   top          -> center top
 *   center       -> center center
 *   left top     -> left top
 *   top left     -> left top
 *
 * Percentages and lengths are deferred; this bridge exists so standard
 * `object-position` can be rewritten onto a parser-safe property without
 * adding any new computed-style integer fields.
 */

#include <assert.h>
#include <string.h>

#include "bytecode/bytecode.h"
#include "bytecode/opcodes.h"
#include "parse/properties/properties.h"
#include "parse/properties/utils.h"

static uint8_t
macsurf_object_position_pack(uint8_t x, uint8_t y)
{
	return (uint8_t)(((x & 0x3) << 2) | (y & 0x3));
}

enum macsurf_object_position_token_kind {
	MACSURF_OBJECT_POS_TOK_INVALID = 0,
	MACSURF_OBJECT_POS_TOK_HORIZ,
	MACSURF_OBJECT_POS_TOK_VERT,
	MACSURF_OBJECT_POS_TOK_CENTER
};

css_error css__parse_macsurf_object_position(css_language *c,
		const parserutils_vector *vector, int32_t *ctx,
		css_style *result)
{
	int32_t orig_ctx = *ctx;
	css_error error;
	const css_token *token;
	bool match;
	uint8_t x = CSS_MACSURF_OBJECT_POSITION_CENTER;
	uint8_t y = CSS_MACSURF_OBJECT_POSITION_CENTER;
	uint8_t value1 = 0, value2 = 0;
	enum macsurf_object_position_token_kind kind1 =
		MACSURF_OBJECT_POS_TOK_INVALID;
	enum macsurf_object_position_token_kind kind2 =
		MACSURF_OBJECT_POS_TOK_INVALID;
	int consumed = 0;

	token = parserutils_vector_iterate(vector, ctx);
	if ((token == NULL) || (token->type != CSS_TOKEN_IDENT)) {
		*ctx = orig_ctx;
		return CSS_INVALID;
	}

	if ((lwc_string_caseless_isequal(
			token->idata, c->strings[INHERIT],
			&match) == lwc_error_ok && match)) {
		return css_stylesheet_style_inherit(result,
				CSS_PROP_MACSURF_OBJECT_POSITION);
	}
	if ((lwc_string_caseless_isequal(
			token->idata, c->strings[INITIAL],
			&match) == lwc_error_ok && match)) {
		return css_stylesheet_style_initial(result,
				CSS_PROP_MACSURF_OBJECT_POSITION);
	}
	if ((lwc_string_caseless_isequal(
			token->idata, c->strings[REVERT],
			&match) == lwc_error_ok && match)) {
		return css_stylesheet_style_revert(result,
				CSS_PROP_MACSURF_OBJECT_POSITION);
	}
	if ((lwc_string_caseless_isequal(
			token->idata, c->strings[UNSET],
			&match) == lwc_error_ok && match)) {
		return css_stylesheet_style_unset(result,
				CSS_PROP_MACSURF_OBJECT_POSITION);
	}

	*ctx = orig_ctx;
	while (consumed < 2) {
		token = parserutils_vector_peek(vector, *ctx);
		if ((token == NULL) || (token->type != CSS_TOKEN_IDENT)) {
			break;
		}

		if ((lwc_string_caseless_isequal(token->idata,
				c->strings[LIBCSS_LEFT],
				&match) == lwc_error_ok && match)) {
			if (consumed == 0) {
				kind1 = MACSURF_OBJECT_POS_TOK_HORIZ;
				value1 = CSS_MACSURF_OBJECT_POSITION_START;
			} else {
				kind2 = MACSURF_OBJECT_POS_TOK_HORIZ;
				value2 = CSS_MACSURF_OBJECT_POSITION_START;
			}
		} else if ((lwc_string_caseless_isequal(token->idata,
				c->strings[LIBCSS_RIGHT],
				&match) == lwc_error_ok && match)) {
			if (consumed == 0) {
				kind1 = MACSURF_OBJECT_POS_TOK_HORIZ;
				value1 = CSS_MACSURF_OBJECT_POSITION_END;
			} else {
				kind2 = MACSURF_OBJECT_POS_TOK_HORIZ;
				value2 = CSS_MACSURF_OBJECT_POSITION_END;
			}
		} else if ((lwc_string_caseless_isequal(token->idata,
				c->strings[TOP],
				&match) == lwc_error_ok && match)) {
			if (consumed == 0) {
				kind1 = MACSURF_OBJECT_POS_TOK_VERT;
				value1 = CSS_MACSURF_OBJECT_POSITION_START;
			} else {
				kind2 = MACSURF_OBJECT_POS_TOK_VERT;
				value2 = CSS_MACSURF_OBJECT_POSITION_START;
			}
		} else if ((lwc_string_caseless_isequal(token->idata,
				c->strings[BOTTOM],
				&match) == lwc_error_ok && match)) {
			if (consumed == 0) {
				kind1 = MACSURF_OBJECT_POS_TOK_VERT;
				value1 = CSS_MACSURF_OBJECT_POSITION_END;
			} else {
				kind2 = MACSURF_OBJECT_POS_TOK_VERT;
				value2 = CSS_MACSURF_OBJECT_POSITION_END;
			}
		} else if ((lwc_string_caseless_isequal(token->idata,
				c->strings[CENTER],
				&match) == lwc_error_ok && match)) {
			if (consumed == 0) {
				kind1 = MACSURF_OBJECT_POS_TOK_CENTER;
				value1 = CSS_MACSURF_OBJECT_POSITION_CENTER;
			} else {
				kind2 = MACSURF_OBJECT_POS_TOK_CENTER;
				value2 = CSS_MACSURF_OBJECT_POSITION_CENTER;
			}
		} else {
			break;
		}

		parserutils_vector_iterate(vector, ctx);
		consumed++;
	}

	if (consumed == 0) {
		*ctx = orig_ctx;
		return CSS_INVALID;
	}

	if (consumed == 1) {
		if (kind1 == MACSURF_OBJECT_POS_TOK_HORIZ) {
			x = value1;
		} else if (kind1 == MACSURF_OBJECT_POS_TOK_VERT) {
			y = value1;
		} else if (kind1 != MACSURF_OBJECT_POS_TOK_CENTER) {
			*ctx = orig_ctx;
			return CSS_INVALID;
		}
	} else {
		if (kind1 == MACSURF_OBJECT_POS_TOK_CENTER &&
				kind2 == MACSURF_OBJECT_POS_TOK_CENTER) {
			/* defaults already correct */
		} else if (kind1 == MACSURF_OBJECT_POS_TOK_HORIZ &&
				kind2 == MACSURF_OBJECT_POS_TOK_VERT) {
			x = value1;
			y = value2;
		} else if (kind1 == MACSURF_OBJECT_POS_TOK_VERT &&
				kind2 == MACSURF_OBJECT_POS_TOK_HORIZ) {
			x = value2;
			y = value1;
		} else if (kind1 == MACSURF_OBJECT_POS_TOK_CENTER &&
				kind2 == MACSURF_OBJECT_POS_TOK_HORIZ) {
			x = value2;
		} else if (kind1 == MACSURF_OBJECT_POS_TOK_CENTER &&
				kind2 == MACSURF_OBJECT_POS_TOK_VERT) {
			y = value2;
		} else if (kind1 == MACSURF_OBJECT_POS_TOK_HORIZ &&
				kind2 == MACSURF_OBJECT_POS_TOK_CENTER) {
			x = value1;
		} else if (kind1 == MACSURF_OBJECT_POS_TOK_VERT &&
				kind2 == MACSURF_OBJECT_POS_TOK_CENTER) {
			y = value1;
		} else {
			*ctx = orig_ctx;
			return CSS_INVALID;
		}
	}

	error = css__stylesheet_style_appendOPV(result,
			CSS_PROP_MACSURF_OBJECT_POSITION,
			0, macsurf_object_position_pack(x, y));
	if (error != CSS_OK) {
		*ctx = orig_ctx;
	}

	return error;
}
