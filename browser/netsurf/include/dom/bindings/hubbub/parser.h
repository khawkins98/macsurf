/*
 * MacSurf — dom/bindings/hubbub/parser.h (netsurf/include mirror)
 *
 * Duplicate of browser/libdom/include/dom/bindings/hubbub/parser.h so that
 * the browser:netsurf:include: access path (which CW8 always has) resolves
 * <dom/bindings/hubbub/parser.h>.
 */
#ifndef dom_hubbub_parser_h_
#define dom_hubbub_parser_h_

#include <stddef.h>

#include <dom/dom.h>
#include <dom/bindings/hubbub/errors.h>

typedef dom_hubbub_error (*dom_script)(void *ctx, struct dom_node *node);

typedef struct dom_hubbub_parser dom_hubbub_parser;

typedef enum dom_hubbub_encoding_source {
	DOM_HUBBUB_ENCODING_SOURCE_HEADER,
	DOM_HUBBUB_ENCODING_SOURCE_DETECTED,
	DOM_HUBBUB_ENCODING_SOURCE_META
} dom_hubbub_encoding_source;

typedef struct dom_hubbub_parser_params {
	const char *enc;
	bool fix_enc;
	bool enable_script;
	dom_script script;
	dom_msg msg;
	void *ctx;
	dom_events_default_action_fetcher daf;
} dom_hubbub_parser_params;

dom_hubbub_error dom_hubbub_parser_create(dom_hubbub_parser_params *params,
		dom_hubbub_parser **parser,
		dom_document **document);

dom_hubbub_error dom_hubbub_fragment_parser_create(
		dom_hubbub_parser_params *params,
		dom_document *document,
		dom_hubbub_parser **parser,
		dom_document_fragment **fragment);

void dom_hubbub_parser_destroy(dom_hubbub_parser *parser);

dom_hubbub_error dom_hubbub_parser_parse_chunk(dom_hubbub_parser *parser,
		const uint8_t *data, size_t len);

dom_hubbub_error dom_hubbub_parser_insert_chunk(dom_hubbub_parser *parser,
		const uint8_t *data, size_t length);

dom_hubbub_error dom_hubbub_parser_completed(dom_hubbub_parser *parser);

const char *dom_hubbub_parser_get_encoding(dom_hubbub_parser *parser,
		dom_hubbub_encoding_source *source);

dom_hubbub_error dom_hubbub_parser_pause(dom_hubbub_parser *parser, bool pause);

#endif
