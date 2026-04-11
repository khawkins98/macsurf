/*
 * MacSurf wrapper header — dom/bindings/hubbub/parser.h
 *
 * NetSurf core does #include <dom/bindings/hubbub/parser.h> assuming
 * libdom has been "installed" with bindings under ${PREFIX}/include/dom/.
 * In our vendored layout the binding header lives at
 * browser/libdom/bindings/hubbub/parser.h, which is on the libdom/bindings
 * search path but not under <dom/...>.
 *
 * This wrapper sits at the dom/-namespaced location and forwards to the
 * real header via the libdom internal-search-path entry that we already
 * have in MacSurf.mcp ({Project}/../../../../libdom/bindings).
 */
#ifndef MACSURF_DOM_BINDINGS_HUBBUB_PARSER_H
#define MACSURF_DOM_BINDINGS_HUBBUB_PARSER_H

#include "../../../../bindings/hubbub/parser.h"

#endif
