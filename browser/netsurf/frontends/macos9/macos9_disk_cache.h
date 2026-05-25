/*
 * MacSurf - macos9_disk_cache.h
 *
 * Persistent on-disk body cache shared by the HTTP and HTTPS fetchers.
 * One file per cached body in the "MacSurf Cache" folder on the boot
 * Desktop; filename is a hash of the URL. See macos9_disk_cache.c for
 * the disk format and write discipline.
 *
 * Each fetcher owns its own in-memory capture buffer (size-bounded,
 * geometric growth) and calls macos9_cache_store() once the response
 * is complete. Cache lookups happen at fetch-start time and let the
 * fetcher short-circuit straight to FETCH_FINISHED without touching
 * the network.
 */

#ifndef MACOS9_DISK_CACHE_H
#define MACOS9_DISK_CACHE_H

#include <stddef.h>

/* Single-response cap. Bigger bodies are served live, not cached. */
#define MACSURF_CACHE_MAX_BYTES (1L * 1024L * 1024L)

/* When non-zero, the next cache_lookup short-circuits to "miss" so
 * the Reload button forces a fresh fetch. cache_store clears the flag
 * after the new body is written, so subsequent sub-resource fetches
 * resume normal cache behaviour. Defined in macos9_disk_cache.c. */
extern int macsurf_http_skip_next_cache;

/* Returns 1 if this (status, mime) pair is worth persisting. */
int macos9_cache_mime_eligible(int status, const char *mime);

/* Try to satisfy a fetch from the on-disk cache. Returns 1 on hit,
 * 0 on miss / I/O error. On hit, *body_out is a malloc'd buffer the
 * caller must free; *body_len_out is the byte count; mime_out (cap
 * >= 128) receives the stored MIME string; *status_out is the HTTP
 * status from the cached response. */
int macos9_cache_lookup(const char *url, char **body_out,
                        long *body_len_out, char *mime_out,
                        int mime_cap, int *status_out);

/* Persist one response. body_ptr is body_len bytes of unmodified
 * response body. Errors are silent (best-effort). */
void macos9_cache_store(const char *url, int status, const char *mime,
                        const char *body_ptr, long body_len);

#endif /* MACOS9_DISK_CACHE_H */
