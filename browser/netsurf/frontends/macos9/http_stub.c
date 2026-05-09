/*
 * MacSurf — http_stub.c
 *
 * The real netsurf/utils/http/cache-control.c, content-type.c, and
 * strict-transport-security.c provide most of the HTTP header parsing
 * the browser needs. content-disposition and www-authenticate also
 * have real upstream implementations but they aren't in the project
 * file list yet, so keep no-op stubs for those two until they are.
 *
 * Licensed under GPL v2.
 */

#include <stddef.h>
#include "utils/errors.h"

struct http_content_disposition;
struct http_www_authenticate;

/* Content-Disposition */
nserror http_parse_content_disposition(const char *header,
		struct http_content_disposition **result)
{
	(void)header;
	if (result) *result = NULL;
	return NSERROR_NOT_FOUND;
}

void http_content_disposition_destroy(
		struct http_content_disposition *cd) { (void)cd; }

/* WWW-Authenticate */
nserror http_parse_www_authenticate(const char *header,
		struct http_www_authenticate **result)
{
	(void)header;
	if (result) *result = NULL;
	return NSERROR_NOT_FOUND;
}

void http_www_authenticate_destroy(
		struct http_www_authenticate *wa) { (void)wa; }
