/*
 * MacSurf — misc_stub.c
 * Stub implementations for NetSurf subsystems whose real .c files
 * are not yet linked into the project. Anything that has a real
 * implementation linked is removed from here to avoid duplicate
 * symbol errors.
 * Licensed under GPL v2.
 */

#include <stddef.h>
#include "utils/errors.h"

struct nsurl;
struct bitmap;
struct cert_chain;
struct download_context;
struct gui_download_window;
struct llcache_handle;
struct image_cache_parameters;

/* image_cache_init / image_cache_fini — image content handler files
 * not yet linked. */
nserror image_cache_init(const struct image_cache_parameters *p)
{
	(void)p;
	return NSERROR_OK;
}

nserror image_cache_fini(void) { return NSERROR_OK; }

/* DOM namespace — NetSurf core calls dom_namespace_initialise (public).
 * libdom's namespace.c provides dom_namespace_finalise; the public
 * dom_namespace_initialise has no real definition we link, so stub it. */
nserror dom_namespace_initialise(void) { return NSERROR_OK; }

/* Content handler init — image / textplain handlers not yet linked. */
nserror textplain_init(void) { return NSERROR_OK; }
nserror image_init(void) { return NSERROR_OK; }

/* nsutils base64 — used only by ssl_certs.c for cert query strings.
 * MacSurf strips TLS at the proxy, so cert chain queries never fire.
 * Returns BAD_INPUT (NSUERROR=2). */
int nsu_base64_encode_url(const unsigned char *input, unsigned long input_length,
		unsigned char **output, unsigned long *output_length)
{
	(void)input; (void)input_length; (void)output; (void)output_length;
	return 2;
}
