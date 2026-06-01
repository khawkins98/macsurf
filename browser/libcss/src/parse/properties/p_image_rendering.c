/*
 * This file is part of LibCSS.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2026 MacSurf (fixes356)
 *
 * Parse image-rendering (#78).
 *
 * Accepted forms:
 *   image-rendering: auto         (default)
 *   image-rendering: smooth       (alias for auto)
 *   image-rendering: high-quality (alias for auto)
 *   image-rendering: crisp-edges  (preserve sharp pixel boundaries)
 *   image-rendering: pixelated    (nearest-neighbor for retro pixel art)
 *
 * Inherits.
 *
 * MacSurf consumption (deferred): when value is CRISP_EDGES or
 * PIXELATED, the plotter's box-filter pre-downscale (fixes203) should
 * skip so the image lands via pure QuickDraw nearest-neighbor
 * CopyBits. This V1 only lands the libcss side; the bitmap plotter
 * wiring is queued as a follow-on round.
 */

#include <assert.h>
#include <string.h>

#include "bytecode/bytecode.h"
#include "bytecode/opcodes.h"
#include "parse/properties/properties.h"
#include "parse/properties/utils.h"

css_error css__parse_image_rendering(css_language *c,
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
				CSS_PROP_IMAGE_RENDERING);
	} else if (lwc_string_caseless_isequal(token->idata,
			c->strings[INITIAL], &match) == lwc_error_ok && match) {
		error = css_stylesheet_style_initial(result,
				CSS_PROP_IMAGE_RENDERING);
	} else if (lwc_string_caseless_isequal(token->idata,
			c->strings[UNSET], &match) == lwc_error_ok && match) {
		error = css_stylesheet_style_unset(result,
				CSS_PROP_IMAGE_RENDERING);
	} else if (lwc_string_caseless_isequal(token->idata,
			c->strings[AUTO], &match) == lwc_error_ok && match) {
		error = css__stylesheet_style_appendOPV(result,
				CSS_PROP_IMAGE_RENDERING, 0,
				IMAGE_RENDERING_AUTO);
	} else if (lwc_string_caseless_isequal(token->idata,
			c->strings[SMOOTH], &match) == lwc_error_ok && match) {
		error = css__stylesheet_style_appendOPV(result,
				CSS_PROP_IMAGE_RENDERING, 0,
				IMAGE_RENDERING_AUTO);
	} else if (lwc_string_caseless_isequal(token->idata,
			c->strings[HIGH_QUALITY],
			&match) == lwc_error_ok && match) {
		error = css__stylesheet_style_appendOPV(result,
				CSS_PROP_IMAGE_RENDERING, 0,
				IMAGE_RENDERING_AUTO);
	} else if (lwc_string_caseless_isequal(token->idata,
			c->strings[CRISP_EDGES],
			&match) == lwc_error_ok && match) {
		error = css__stylesheet_style_appendOPV(result,
				CSS_PROP_IMAGE_RENDERING, 0,
				IMAGE_RENDERING_CRISP_EDGES);
	} else if (lwc_string_caseless_isequal(token->idata,
			c->strings[PIXELATED],
			&match) == lwc_error_ok && match) {
		error = css__stylesheet_style_appendOPV(result,
				CSS_PROP_IMAGE_RENDERING, 0,
				IMAGE_RENDERING_PIXELATED);
	} else {
		*ctx = orig_ctx;
		return CSS_INVALID;
	}

	if (error != CSS_OK) {
		*ctx = orig_ctx;
	}
	return error;
}
