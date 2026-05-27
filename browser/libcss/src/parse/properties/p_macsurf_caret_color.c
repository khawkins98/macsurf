/*
 * fixes287 stub: caret-color libcss vendor property was reverted.
 * This file exists only so MacSurf.mcp keeps linking. The parser
 * always returns CSS_INVALID; libcss drops the declaration.
 */

#include "parse/properties/properties.h"

css_error css__parse_macsurf_caret_color(css_language *c,
		const parserutils_vector *vector, int32_t *ctx,
		css_style *result)
{
	(void)c;
	(void)vector;
	(void)ctx;
	(void)result;
	return CSS_INVALID;
}
