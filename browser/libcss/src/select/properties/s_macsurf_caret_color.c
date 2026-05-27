/*
 * fixes287 stub: caret-color libcss vendor property was reverted.
 * This file exists only so MacSurf.mcp keeps linking. All functions
 * are no-op stubs.
 */

#include <libcss/types.h>
#include "select/properties/properties.h"

css_error css__cascade_macsurf_caret_color(uint32_t opv,
		css_style *style, css_select_state *state)
{
	(void)opv; (void)style; (void)state;
	return CSS_OK;
}

css_error css__set_macsurf_caret_color_from_hint(const css_hint *hint,
		css_computed_style *style)
{
	(void)hint; (void)style;
	return CSS_OK;
}

css_error css__initial_macsurf_caret_color(css_select_state *state)
{
	(void)state;
	return CSS_OK;
}

css_error css__copy_macsurf_caret_color(
		const css_computed_style *from,
		css_computed_style *to)
{
	(void)from; (void)to;
	return CSS_OK;
}

css_error css__compose_macsurf_caret_color(
		const css_computed_style *parent,
		const css_computed_style *child,
		css_computed_style *result)
{
	(void)parent; (void)child; (void)result;
	return CSS_OK;
}

uint32_t destroy_macsurf_caret_color(void *bytecode)
{
	(void)bytecode;
	return 0;
}
