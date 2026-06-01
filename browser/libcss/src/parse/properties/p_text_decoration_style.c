/*
 * This file is part of LibCSS.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2026 MacSurf (fixes357)
 *
 * Parse text-decoration-style (#44).
 *
 * Accepted forms:
 *   text-decoration-style: solid (default) | double | dotted | dashed | wavy
 *
 * Not inherited.
 *
 * Storage in css_computed_style_i.text_decoration_style (int32_t):
 *   CSS_TEXT_DECORATION_STYLE_*
 */

#include <assert.h>
#include <string.h>

#include "bytecode/bytecode.h"
#include "bytecode/opcodes.h"
#include "parse/properties/properties.h"
#include "parse/properties/utils.h"

css_error css__parse_text_decoration_style(css_language *c,
		const parserutils_vector *vector, int32_t *ctx,
		css_style *result)
{
	int32_t orig_ctx = *ctx;
	css_error error;
	const css_token *token;
	bool match;

	token = parserutils_vector_iterate(vector, ctx);
	if (token == NULL || token->type != CSS_TOKEN_IDENT) {
		*ctx = orig_ctx;
		return CSS_INVALID;
	}

	if (lwc_string_caseless_isequal(token->idata,
			c->strings[INHERIT], &match) == lwc_error_ok && match) {
		error = css_stylesheet_style_inherit(result,
				CSS_PROP_TEXT_DECORATION_STYLE);
	} else if (lwc_string_caseless_isequal(token->idata,
			c->strings[INITIAL], &match) == lwc_error_ok && match) {
		error = css_stylesheet_style_initial(result,
				CSS_PROP_TEXT_DECORATION_STYLE);
	} else if (lwc_string_caseless_isequal(token->idata,
			c->strings[UNSET], &match) == lwc_error_ok && match) {
		error = css_stylesheet_style_unset(result,
				CSS_PROP_TEXT_DECORATION_STYLE);
	} else if (lwc_string_caseless_isequal(token->idata,
			c->strings[SOLID], &match) == lwc_error_ok && match) {
		error = css__stylesheet_style_appendOPV(result,
				CSS_PROP_TEXT_DECORATION_STYLE, 0,
				TEXT_DECORATION_STYLE_SOLID);
	} else if (lwc_string_caseless_isequal(token->idata,
			c->strings[LIBCSS_DOUBLE],
			&match) == lwc_error_ok && match) {
		error = css__stylesheet_style_appendOPV(result,
				CSS_PROP_TEXT_DECORATION_STYLE, 0,
				TEXT_DECORATION_STYLE_DOUBLE);
	} else if (lwc_string_caseless_isequal(token->idata,
			c->strings[DOTTED], &match) == lwc_error_ok && match) {
		error = css__stylesheet_style_appendOPV(result,
				CSS_PROP_TEXT_DECORATION_STYLE, 0,
				TEXT_DECORATION_STYLE_DOTTED);
	} else if (lwc_string_caseless_isequal(token->idata,
			c->strings[DASHED], &match) == lwc_error_ok && match) {
		error = css__stylesheet_style_appendOPV(result,
				CSS_PROP_TEXT_DECORATION_STYLE, 0,
				TEXT_DECORATION_STYLE_DASHED);
	} else if (lwc_string_caseless_isequal(token->idata,
			c->strings[WAVY], &match) == lwc_error_ok && match) {
		error = css__stylesheet_style_appendOPV(result,
				CSS_PROP_TEXT_DECORATION_STYLE, 0,
				TEXT_DECORATION_STYLE_WAVY);
	} else {
		*ctx = orig_ctx;
		return CSS_INVALID;
	}

	if (error != CSS_OK) {
		*ctx = orig_ctx;
	}
	return error;
}
