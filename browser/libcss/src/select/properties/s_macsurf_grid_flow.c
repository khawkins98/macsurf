/*
 * This file is part of LibCSS
 * Licensed under the MIT License,
 *                http://www.opensource.org/licenses/mit-license.php
 * Copyright 2026 MacSurf (fixes275, #65)
 *
 * Cascade -macsurf-grid-flow. Stores the enum value directly in the
 * inner _i struct as int32 (memcmp-safe, self-aligning). Default 0
 * means "no grid-auto-flow set"; layout treats this as the spec
 * default (row, sparse row-major).
 *
 * Per-container, NOT inherited. Child grid items should not pick up
 * grid-auto-flow from a parent grid container (compose returns the
 * child's value verbatim).
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

css_error css__cascade_macsurf_grid_flow(uint32_t opv,
		css_style *style, css_select_state *state)
{
	int32_t flow = 0;
	bool is_set = false;

	if (hasFlagValue(opv) == false) {
		if (getValue(opv) == 0x0080) { /* SET */
			is_set = true;
			flow = *((css_fixed *) style->bytecode);
			advance_bytecode(style, sizeof(css_fixed));
			if (flow < 0 || flow > 4) flow = 0;
		}
	}

	if (css__outranks_existing(getOpcode(opv), isImportant(opv), state,
			getFlagValue(opv))) {
		state->computed->i.macsurf_grid_flow = is_set ? flow : 0;
	}

	return CSS_OK;
}

css_error css__set_macsurf_grid_flow_from_hint(const css_hint *hint,
		css_computed_style *style)
{
	(void)hint;
	style->i.macsurf_grid_flow = 0;
	return CSS_OK;
}

css_error css__initial_macsurf_grid_flow(css_select_state *state)
{
	state->computed->i.macsurf_grid_flow = 0;
	return CSS_OK;
}

css_error css__copy_macsurf_grid_flow(
		const css_computed_style *from,
		css_computed_style *to)
{
	if (from == to) return CSS_OK;
	to->i.macsurf_grid_flow = from->i.macsurf_grid_flow;
	return CSS_OK;
}

css_error css__compose_macsurf_grid_flow(
		const css_computed_style *parent,
		const css_computed_style *child,
		css_computed_style *result)
{
	(void)parent;
	result->i.macsurf_grid_flow = child->i.macsurf_grid_flow;
	return CSS_OK;
}

uint32_t destroy_macsurf_grid_flow(void *bytecode)
{
	(void)bytecode;
	return 0;
}
