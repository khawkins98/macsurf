/*
 * This file is part of LibCSS
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2026 MacSurf (fixes356)
 *
 * Cascade image-rendering (#78). Inherits. Self-aligning int32_t.
 */

#include <stdlib.h>
#include <string.h>

#include "bytecode/bytecode.h"
#include "bytecode/opcodes.h"
#include "select/propset.h"
#include "select/propget.h"
#include "utils/utils.h"

#include "select/properties/properties.h"
#include "select/properties/helpers.h"

css_error css__cascade_image_rendering(uint32_t opv, css_style *style,
		css_select_state *state)
{
	int32_t v = CSS_IMAGE_RENDERING_INHERIT;
	(void)style;

	if (hasFlagValue(opv) == false) {
		switch (getValue(opv)) {
		case IMAGE_RENDERING_AUTO:
			v = CSS_IMAGE_RENDERING_AUTO;
			break;
		case IMAGE_RENDERING_CRISP_EDGES:
			v = CSS_IMAGE_RENDERING_CRISP_EDGES;
			break;
		case IMAGE_RENDERING_PIXELATED:
			v = CSS_IMAGE_RENDERING_PIXELATED;
			break;
		}
	}

	if (css__outranks_existing(getOpcode(opv), isImportant(opv), state,
			getFlagValue(opv))) {
		state->computed->i.image_rendering = v;
	}

	return CSS_OK;
}

css_error css__set_image_rendering_from_hint(const css_hint *hint,
		css_computed_style *style)
{
	style->i.image_rendering = hint->status;
	return CSS_OK;
}

css_error css__initial_image_rendering(css_select_state *state)
{
	state->computed->i.image_rendering = CSS_IMAGE_RENDERING_AUTO;
	return CSS_OK;
}

css_error css__copy_image_rendering(
		const css_computed_style *from,
		css_computed_style *to)
{
	if (from == to) return CSS_OK;
	to->i.image_rendering = from->i.image_rendering;
	return CSS_OK;
}

css_error css__compose_image_rendering(
		const css_computed_style *parent,
		const css_computed_style *child,
		css_computed_style *result)
{
	int32_t v = child->i.image_rendering;
	if (v == CSS_IMAGE_RENDERING_INHERIT) {
		v = parent->i.image_rendering;
	}
	result->i.image_rendering = v;
	return CSS_OK;
}

uint32_t destroy_image_rendering(void *bytecode)
{
	(void)bytecode;
	return 0;
}
