/*
 * This file is part of LibCSS.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2026 MacSurf (fixes71)
 *
 * Parse -macsurf-transform.
 *
 * Accepted forms (V1):
 *   -macsurf-transform: none
 *   -macsurf-transform: rotate(<angle>)
 *   -macsurf-transform: translate(<x> [, <y>])
 *   -macsurf-transform: rotate(<angle>) translate(<x>, <y>)
 *
 * <angle> is a number followed by "deg"; bare numbers treated as deg.
 * <x>, <y> are integer pixel values (negative allowed).
 *
 * V1 storage (compact int32_t in plot_style_t):
 *   bits 31..16: rotation angle in Q10.6 fixed-point degrees
 *                (range ±512°, 1/64° precision, treated mod 360)
 *   bits 15..8:  translate-x signed int8 (-128..127 px)
 *   bits 7..0:   translate-y signed int8 (-128..127 px)
 *
 * scale(), skew(), matrix(), translateX/Y, scaleX/Y, skewX/Y, and
 * non-multiple-of-90 rotations are accepted at the parser but
 * silently ignored (their tokens are consumed; the storage stays
 * at identity for those components).  V2 will add full matrix
 * composition.
 *
 * Bytecode payload after appendOPV(SET):
 *   - css_fixed rotation_deg (Q22.10 internal)
 *   - css_fixed translate_x_px
 *   - css_fixed translate_y_px
 */

#include <assert.h>
#include <string.h>

#include "bytecode/bytecode.h"
#include "bytecode/opcodes.h"
#include "parse/properties/properties.h"
#include "parse/properties/utils.h"

/* Skip the contents of an unrecognised function — read past matching
 * parenthesis pair so the outer loop can continue with the next
 * function in the transform list. */
static void skip_function_args(const parserutils_vector *vector, int32_t *ctx)
{
	int depth = 1;
	const css_token *t;

	while (depth > 0) {
		t = parserutils_vector_iterate(vector, ctx);
		if (t == NULL) return;
		if (t->type == CSS_TOKEN_CHAR && t->idata != NULL) {
			const char *s = lwc_string_data(t->idata);
			if (s != NULL) {
				if (s[0] == '(') depth++;
				else if (s[0] == ')') depth--;
			}
		}
	}
}

css_error css__parse_macsurf_transform(css_language *c,
		const parserutils_vector *vector, int32_t *ctx,
		css_style *result)
{
	int32_t orig_ctx = *ctx;
	css_error error;
	const css_token *token;
	enum flag_value flag_value;
	css_fixed rotation = 0;     /* Q22.10 deg */
	css_fixed tx = 0;           /* Q22.10 px  */
	css_fixed ty = 0;           /* Q22.10 px  */
	/* fixes73: scale defaults to 1.0 (identity). Stored in Q22.10
	 * for consistency with the other css_fixed values; cascade
	 * packs both into the macsurf_transform_b storage slot. */
	css_fixed scale_x = (css_fixed)(1 << 10);  /* 1.0 in Q22.10 */
	css_fixed scale_y = (css_fixed)(1 << 10);
	uint32_t unit = 0;
	int got_any = 0;

	token = parserutils_vector_peek(vector, *ctx);
	if (token == NULL) return CSS_INVALID;

	flag_value = get_css_flag_value(c, token);
	if (flag_value != FLAG_VALUE__NONE) {
		parserutils_vector_iterate(vector, ctx);
		return css_stylesheet_style_flag_value(result, flag_value,
				CSS_PROP_MACSURF_TRANSFORM);
	}

	consumeWhitespace(vector, ctx);

	/* `none` keyword */
	token = parserutils_vector_peek(vector, *ctx);
	if (token != NULL && token->type == CSS_TOKEN_IDENT &&
			token->idata != NULL) {
		bool match = false;
		if (lwc_string_caseless_isequal(token->idata,
				c->strings[NONE], &match) == lwc_error_ok &&
				match) {
			parserutils_vector_iterate(vector, ctx);
			return css__stylesheet_style_appendOPV(result,
					CSS_PROP_MACSURF_TRANSFORM, 0,
					0x0000 /* NONE */);
		}
	}

	/* One or more transform functions. Walk them; recognise rotate,
	 * translate, scale, plus the X/Y variants. skew/matrix are V4+. */
	while (1) {
		const char *fname;
		bool is_rotate = false;
		bool is_translate = false;
		bool is_scale = false;     /* fixes73 */
		bool scale_axis_x = false; /* scaleX only */
		bool scale_axis_y = false; /* scaleY only */

		consumeWhitespace(vector, ctx);

		token = parserutils_vector_peek(vector, *ctx);
		if (token == NULL) break;
		if (token->type != CSS_TOKEN_FUNCTION) break;
		if (token->idata == NULL) break;

		fname = lwc_string_data(token->idata);
		if (fname == NULL) break;

		/* Identify the function by case-insensitive name. The token
		 * idata includes the trailing `(` per libcss convention, so
		 * compare without the paren. */
		if (strncasecmp(fname, "rotate", 6) == 0 &&
				(fname[6] == '(' || fname[6] == '\0')) {
			is_rotate = true;
		} else if (strncasecmp(fname, "translate", 9) == 0 &&
				(fname[9] == '(' || fname[9] == '\0')) {
			is_translate = true;
		} else if (strncasecmp(fname, "scaleX", 6) == 0 &&
				(fname[6] == '(' || fname[6] == '\0')) {
			is_scale = true; scale_axis_x = true;
		} else if (strncasecmp(fname, "scaleY", 6) == 0 &&
				(fname[6] == '(' || fname[6] == '\0')) {
			is_scale = true; scale_axis_y = true;
		} else if (strncasecmp(fname, "scale", 5) == 0 &&
				(fname[5] == '(' || fname[5] == '\0')) {
			is_scale = true;
		}
		/* translateX/Y, skew*, matrix → silently ignored
		 * for V3. Consume tokens up to the matching ')'. */

		parserutils_vector_iterate(vector, ctx);  /* eat function token */

		if (is_rotate) {
			css_fixed angle;
			consumeWhitespace(vector, ctx);
			error = css__parse_unit_specifier(c, vector, ctx,
					UNIT_DEG, &angle, &unit);
			if (error == CSS_OK) {
				rotation = angle;  /* Q22.10 degrees */
				got_any = 1;
			}
			/* skip until ')' */
			consumeWhitespace(vector, ctx);
			token = parserutils_vector_iterate(vector, ctx);
			while (token != NULL) {
				if (token->type == CSS_TOKEN_CHAR &&
						token->idata != NULL) {
					const char *s = lwc_string_data(token->idata);
					if (s != NULL && s[0] == ')') break;
				}
				token = parserutils_vector_iterate(vector, ctx);
			}
		} else if (is_translate) {
			css_fixed v1 = 0, v2 = 0;
			consumeWhitespace(vector, ctx);
			error = css__parse_unit_specifier(c, vector, ctx,
					UNIT_PX, &v1, &unit);
			if (error == CSS_OK) {
				tx = v1;
				consumeWhitespace(vector, ctx);
				token = parserutils_vector_peek(vector, *ctx);
				if (token != NULL && token->type == CSS_TOKEN_CHAR &&
						token->idata != NULL) {
					const char *s = lwc_string_data(token->idata);
					if (s != NULL && s[0] == ',') {
						parserutils_vector_iterate(vector, ctx);
						consumeWhitespace(vector, ctx);
						error = css__parse_unit_specifier(
								c, vector, ctx,
								UNIT_PX, &v2,
								&unit);
						if (error == CSS_OK) ty = v2;
					}
				}
				got_any = 1;
			}
			/* skip until ')' */
			consumeWhitespace(vector, ctx);
			token = parserutils_vector_iterate(vector, ctx);
			while (token != NULL) {
				if (token->type == CSS_TOKEN_CHAR &&
						token->idata != NULL) {
					const char *s = lwc_string_data(token->idata);
					if (s != NULL && s[0] == ')') break;
				}
				token = parserutils_vector_iterate(vector, ctx);
			}
		} else if (is_scale) {
			css_fixed sv1 = 0, sv2 = 0;
			consumeWhitespace(vector, ctx);
			/* scale arg is a number (no unit). Read via the
			 * unit_specifier path; on a bare number CSS gives
			 * a unitless integer multiplied by 1024 (Q22.10),
			 * which matches our scale_x / scale_y format. */
			error = css__parse_unit_specifier(c, vector, ctx,
					UNIT_PCT, &sv1, &unit);
			if (error == CSS_OK) {
				/* Bare number → arrives as Q22.10 with unit
				 * set to NUMBER. PCT path returns Q22.10 in
				 * 0..100 range, so we'd need to divide. Easiest
				 * compromise: treat the parsed value as Q22.10
				 * directly. CSS scale(1.5) parsed as a number
				 * comes through as 1.5 * 1024 = 1536 — correct.
				 * scale(150%) would arrive as 150 * 1024;
				 * divide by 100 to get the proportional form. */
				if ((unit & 0xff) == UNIT_PCT) {
					sv1 = sv1 / 100;
				}
				sv2 = sv1;  /* default uniform */
				consumeWhitespace(vector, ctx);
				token = parserutils_vector_peek(vector, *ctx);
				if (token != NULL && token->type == CSS_TOKEN_CHAR &&
						token->idata != NULL) {
					const char *s = lwc_string_data(token->idata);
					if (s != NULL && s[0] == ',') {
						parserutils_vector_iterate(vector, ctx);
						consumeWhitespace(vector, ctx);
						error = css__parse_unit_specifier(
								c, vector, ctx,
								UNIT_PCT, &sv2,
								&unit);
						if (error == CSS_OK) {
							if ((unit & 0xff) == UNIT_PCT) {
								sv2 = sv2 / 100;
							}
						} else {
							sv2 = sv1;
						}
					}
				}
				if (scale_axis_x) {
					scale_x = sv1;
				} else if (scale_axis_y) {
					scale_y = sv1;
				} else {
					scale_x = sv1;
					scale_y = sv2;
				}
				got_any = 1;
			}
			/* skip until ')' */
			consumeWhitespace(vector, ctx);
			token = parserutils_vector_iterate(vector, ctx);
			while (token != NULL) {
				if (token->type == CSS_TOKEN_CHAR &&
						token->idata != NULL) {
					const char *s = lwc_string_data(token->idata);
					if (s != NULL && s[0] == ')') break;
				}
				token = parserutils_vector_iterate(vector, ctx);
			}
		} else {
			/* Unknown / unsupported function — consume to ')' */
			skip_function_args(vector, ctx);
			got_any = 1;  /* still counts as a valid transform decl */
		}
	}

	if (!got_any) {
		*ctx = orig_ctx;
		return CSS_INVALID;
	}

	error = css__stylesheet_style_appendOPV(result,
			CSS_PROP_MACSURF_TRANSFORM, 0, 0x0080 /* SET */);
	if (error != CSS_OK) return error;

	/* fixes73: append scale_x and scale_y in addition to the
	 * rotation/translate triple. Cascade reads all five. */
	return css__stylesheet_style_vappend(result, 5,
			(css_fixed)rotation, (css_fixed)tx, (css_fixed)ty,
			(css_fixed)scale_x, (css_fixed)scale_y);
}
