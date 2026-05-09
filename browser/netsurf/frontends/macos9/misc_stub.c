/*
 * MacSurf — misc_stub.c
 *
 * After the real netsurf/desktop/* and netsurf/utils/http/* files were
 * added to the project, almost everything this file used to stub now
 * has a real implementation. The only symbol left without an upstream
 * provider is lwc_iterate_strings (libwapcaplet does export it, but
 * the MacSurf shim doesn't pull in libwapcaplet).
 *
 * Licensed under GPL v2.
 */

#include <stddef.h>
#include "utils/errors.h"

struct lwc_string_s;

/* libwapcaplet shim — no per-string iteration on this platform */
void lwc_iterate_strings(void (*cb)(struct lwc_string_s *str, void *pw),
		void *pw) { (void)cb; (void)pw; }
