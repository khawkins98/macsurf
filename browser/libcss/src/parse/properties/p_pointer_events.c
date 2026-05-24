/*
 * This file is part of LibCSS.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2026 Gemini CLI
 */

#include <assert.h>
#include <string.h>

#include "bytecode/bytecode.h"
#include "bytecode/opcodes.h"
#include "parse/properties/properties.h"
#include "parse/properties/utils.h"

/**
 * Parse pointer-events: auto | none | inherit
 */
css_error css__parse_pointer_events(css_language *c,
		const parserutils_vector *vector, int32_t *ctx,
		css_style *result)
{
	int32_t orig_ctx = *ctx;
	const css_token *token;
	uint8_t flags = 0;
	uint16_t value = 0;
	uint32_t opv;

	token = parserutils_vector_peek(vector, *ctx);
	if (token == NULL)
		return CSS_INVALID;

	if (token->type == CSS_TOKEN_IDENT) {
		bool match;

		if ((lwc_string_caseless_isequal(token->idata,
				c->strings[INHERIT], &match) == lwc_error_ok) &&
				match) {
			flags = FLAG_INHERIT;
			parserutils_vector_iterate(vector, ctx);
		} else if ((lwc_string_caseless_isequal(token->idata,
				c->strings[AUTO], &match) == lwc_error_ok) &&
				match) {
			value = CSS_POINTER_EVENTS_AUTO;
			parserutils_vector_iterate(vector, ctx);
		} else if ((lwc_string_caseless_isequal(token->idata,
				c->strings[NONE], &match) == lwc_error_ok) &&
				match) {
			value = CSS_POINTER_EVENTS_NONE;
			parserutils_vector_iterate(vector, ctx);
		} else {
			return CSS_INVALID;
		}
	} else {
		return CSS_INVALID;
	}

	return css__stylesheet_style_appendOPV(result, CSS_PROP_POINTER_EVENTS, flags, value);
}
