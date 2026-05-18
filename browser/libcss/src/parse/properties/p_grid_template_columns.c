/*
 * This file is part of LibCSS.
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2026 MacSurf (fixes112)
 *
 * Parse `grid-template-columns`.
 *
 * Real CSS-Grid grammar is much larger than what MacSurf's V1 grid layout
 * can act on (single int N = number of equal columns + optional rows).
 * Rather than introduce new computed-style storage for the full grammar,
 * we parse the standard property and FOLD it onto the existing
 * `-macsurf-grid` packed value. The select-time path is unchanged: the
 * macsurf_grid storage holds (cols<<16 | rows), and layout_grid.c uses
 * that to lay out children in N equal columns.
 *
 * Patterns recognised — column count derivation:
 *   none / auto / initial / unset        -> cols=1
 *   <track> <track> <track> ...          -> cols = count of tracks
 *   repeat(<int>, <track> [<track> ...]) -> cols = int * inner_count
 *   repeat(auto-fill|auto-fit, ...)      -> cols = 3 * inner_count (heuristic)
 *   single <track>                       -> cols=1
 *
 * <track> is any of: <length>, <percent>, <flex>, <ident> (auto,
 * min-content, max-content), or minmax(...) / fit-content(...) treated
 * as 1 track. Anything we don't recognise still counts as 1 track so
 * the column count stays useful even on unfamiliar tracks.
 *
 * We deliberately drop the per-column width information for now — the
 * V1 grid layout still draws equal columns. Step 3 of the CSS expansion
 * sequence will extend macsurf_grid's packed slot to carry a first-col
 * fixed width when the source said `<length> 1fr` (the dominant
 * mactrove-style sidebar pattern).
 *
 * Bytecode payload: emits the SAME opcode/operand as
 * `css__parse_macsurf_grid` so no new select handler is needed.
 */

#include <assert.h>
#include <string.h>

#include "bytecode/bytecode.h"
#include "bytecode/opcodes.h"
#include "parse/properties/properties.h"
#include "parse/properties/utils.h"

#define AUTO_FILL_FALLBACK_COLUMNS 3

/* Skip a balanced function body. *ctx points at the first token AFTER
 * the FUNCTION (paren-open is part of the FUNCTION token in libcss).
 * Consumes tokens until the matching ')' is consumed. */
static void
skip_function_body(const parserutils_vector *vector, int32_t *ctx)
{
	int depth = 1;
	const css_token *t;
	while (depth > 0 &&
			(t = parserutils_vector_peek(vector, *ctx)) != NULL) {
		if (t->type == CSS_TOKEN_FUNCTION) {
			depth++;
			parserutils_vector_iterate(vector, ctx);
			continue;
		}
		if (t->type == CSS_TOKEN_CHAR && t->idata != NULL &&
				lwc_string_length(t->idata) == 1) {
			char ch = lwc_string_data(t->idata)[0];
			if (ch == '(') {
				depth++;
				parserutils_vector_iterate(vector, ctx);
				continue;
			}
			if (ch == ')') {
				depth--;
				parserutils_vector_iterate(vector, ctx);
				continue;
			}
		}
		if (t->type == CSS_TOKEN_EOF) break;
		parserutils_vector_iterate(vector, ctx);
	}
}

/* Returns true if the token is a value-type that counts as one
 * grid track (numbers, dimensions, percentages, idents). */
static bool
token_is_track_value(const css_token *t)
{
	if (t == NULL) return false;
	switch (t->type) {
	case CSS_TOKEN_NUMBER:
	case CSS_TOKEN_DIMENSION:
	case CSS_TOKEN_PERCENTAGE:
	case CSS_TOKEN_IDENT:
		return true;
	default:
		return false;
	}
}

/* Returns true if the current top-level token signals end-of-declaration. */
static bool
token_is_decl_terminator(const css_token *t)
{
	if (t == NULL || t->type == CSS_TOKEN_EOF) return true;
	if (t->type == CSS_TOKEN_CHAR && t->idata != NULL &&
			lwc_string_length(t->idata) == 1) {
		char ch = lwc_string_data(t->idata)[0];
		if (ch == ';' || ch == '}' || ch == '!') return true;
	}
	return false;
}

/* Count tracks at the current paren depth. Stops at a decl terminator
 * (top level) or a ')' (function body). Returns the track count and
 * leaves *ctx pointing at the terminator (caller consumes it). */
static int
count_tracks(css_language *c,
		const parserutils_vector *vector, int32_t *ctx,
		bool inside_function)
{
	int tracks = 0;
	const css_token *t;

	while ((t = parserutils_vector_peek(vector, *ctx)) != NULL) {
		if (inside_function) {
			if (t->type == CSS_TOKEN_CHAR && t->idata != NULL &&
					lwc_string_length(t->idata) == 1 &&
					lwc_string_data(t->idata)[0] == ')') {
				/* Caller consumes the ')'. */
				return tracks;
			}
		} else {
			if (token_is_decl_terminator(t)) return tracks;
		}

		if (t->type == CSS_TOKEN_S) {
			parserutils_vector_iterate(vector, ctx);
			continue;
		}

		if (t->type == CSS_TOKEN_CHAR && t->idata != NULL &&
				lwc_string_length(t->idata) == 1) {
			char ch = lwc_string_data(t->idata)[0];
			if (ch == ',' || ch == '[' || ch == ']') {
				/* Commas separate args, square brackets
				 * are line-name lists which don't count
				 * as tracks. */
				parserutils_vector_iterate(vector, ctx);
				continue;
			}
		}

		if (t->type == CSS_TOKEN_FUNCTION) {
			bool is_repeat = false;
			if (t->idata != NULL) {
				bool m = false;
				if (lwc_string_caseless_isequal(t->idata,
						c->strings[REPEAT], &m) ==
						lwc_error_ok && m) {
					is_repeat = true;
				}
			}
			parserutils_vector_iterate(vector, ctx);
			if (is_repeat && !inside_function) {
				/* repeat(N, ...) expands inline. Parse N
				 * then inner-track list. */
				int multiplier = 1;
				int inner = 0;
				const css_token *n;
				while ((n = parserutils_vector_peek(
						vector, *ctx)) != NULL &&
						n->type == CSS_TOKEN_S) {
					parserutils_vector_iterate(vector, ctx);
				}
				n = parserutils_vector_peek(vector, *ctx);
				if (n != NULL && n->type == CSS_TOKEN_NUMBER &&
						n->idata != NULL) {
					size_t consumed = 0;
					css_fixed v =
						css__number_from_lwc_string(
							n->idata, true,
							&consumed);
					int iv = (int)(v >> 10);
					if (iv > 0 && iv < 1000) {
						multiplier = iv;
					}
					parserutils_vector_iterate(vector, ctx);
				} else {
					/* auto-fill / auto-fit / unknown:
					 * fall back to a heuristic count so
					 * the grid still has columns. */
					multiplier = AUTO_FILL_FALLBACK_COLUMNS;
					if (n != NULL &&
							n->type ==
								CSS_TOKEN_IDENT) {
						parserutils_vector_iterate(
							vector, ctx);
					}
				}
				/* Consume the comma. */
				while ((n = parserutils_vector_peek(
						vector, *ctx)) != NULL &&
						n->type == CSS_TOKEN_S) {
					parserutils_vector_iterate(vector, ctx);
				}
				n = parserutils_vector_peek(vector, *ctx);
				if (n != NULL && n->type == CSS_TOKEN_CHAR &&
						n->idata != NULL &&
						lwc_string_length(
							n->idata) == 1 &&
						lwc_string_data(
							n->idata)[0] == ',') {
					parserutils_vector_iterate(vector, ctx);
				}
				/* Count inner tracks until the ')'. */
				inner = count_tracks(c, vector, ctx, true);
				if (inner < 1) inner = 1;
				/* Consume the ')'. */
				n = parserutils_vector_peek(vector, *ctx);
				if (n != NULL && n->type == CSS_TOKEN_CHAR &&
						n->idata != NULL &&
						lwc_string_length(
							n->idata) == 1 &&
						lwc_string_data(
							n->idata)[0] == ')') {
					parserutils_vector_iterate(vector, ctx);
				}
				tracks += multiplier * inner;
			} else {
				/* Non-repeat function (minmax, fit-content,
				 * etc.) counts as one track. Skip its body. */
				tracks++;
				skip_function_body(vector, ctx);
			}
			continue;
		}

		if (token_is_track_value(t)) {
			tracks++;
			parserutils_vector_iterate(vector, ctx);
			continue;
		}

		/* Unknown token — consume so we don't loop forever. */
		parserutils_vector_iterate(vector, ctx);
	}

	return tracks;
}


css_error css__parse_grid_template_columns(css_language *c,
		const parserutils_vector *vector, int32_t *ctx,
		css_style *result)
{
	int32_t orig_ctx = *ctx;
	css_error error;
	const css_token *token;
	enum flag_value flag_value;
	int columns;
	uint32_t packed;
	bool match = false;

	token = parserutils_vector_peek(vector, *ctx);
	if (token == NULL) return CSS_INVALID;

	flag_value = get_css_flag_value(c, token);
	if (flag_value != FLAG_VALUE__NONE) {
		parserutils_vector_iterate(vector, ctx);
		return css_stylesheet_style_flag_value(result, flag_value,
				CSS_PROP_MACSURF_GRID);
	}

	consumeWhitespace(vector, ctx);

	/* none / auto collapse to single-column. */
	token = parserutils_vector_peek(vector, *ctx);
	if (token != NULL && token->type == CSS_TOKEN_IDENT &&
			token->idata != NULL) {
		if ((lwc_string_caseless_isequal(token->idata,
				c->strings[NONE], &match) == lwc_error_ok && match) ||
		    (lwc_string_caseless_isequal(token->idata,
				c->strings[AUTO], &match) == lwc_error_ok && match)) {
			parserutils_vector_iterate(vector, ctx);
			columns = 1;
			goto emit;
		}
	}

	columns = count_tracks(c, vector, ctx, false);
	if (columns < 1) columns = 1;
	if (columns > 255) columns = 255;

emit:
	packed = ((uint32_t)columns << 16) | 0u; /* rows = 0 (auto) */

	error = css__stylesheet_style_appendOPV(result,
			CSS_PROP_MACSURF_GRID, 0, 0x0080 /* SET */);
	if (error != CSS_OK) {
		*ctx = orig_ctx;
		return error;
	}

	return css__stylesheet_style_vappend(result, 1, (css_fixed)packed);
}
