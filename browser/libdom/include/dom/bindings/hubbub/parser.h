/*
 * MacSurf wrapper — dom/bindings/hubbub/parser.h
 *
 * Inlined copy of browser/libdom/bindings/hubbub/parser.h.
 * The original wrapper used #include "../../../../bindings/hubbub/parser.h"
 * which CW8 can't resolve. This file IS the content now.
 *
 * Note: the "errors.h" include below resolves via the
 * libdom:bindings:hubbub: access path to the real errors.h
 * at browser/libdom/bindings/hubbub/errors.h.
 */

#ifndef dom_hubbub_parser_h_
#define dom_hubbub_parser_h_

#include <stddef.h>
#include <inttypes.h>

/* errors.h (in this same directory) is self-contained and provides
 * both the HUBBUB constants and dom_hubbub_error without needing
 * <hubbub/errors.h>. dom_node and dom_document come from <dom/dom.h>. */
#include "errors.h"
#include <dom/dom.h>

/**
 * Type of script completion function
 */
typedef dom_hubbub_error (*dom_script)(void *ctx, struct dom_node *node);

typedef struct dom_hubbub_parser dom_hubbub_parser;

/* The encoding source of the document */
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

dom_hubbub_error dom_hubbub_fragment_parser_create(dom_hubbub_parser_params *params,
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
