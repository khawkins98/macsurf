/*
 * MacSurf stub -- shims/inttypes.h
 * C89-compatible printf format macros for CodeWarrior 8.
 *
 * MSL's <inttypes.h> includes <stdint.h> which pulls in <cstdint> (C++).
 * This shim provides format macros directly and includes our stdint.h shim.
 *
 * Licensed under GPL v2.
 */

#ifndef MACOS9_SHIMS_INTTYPES_H
#define MACOS9_SHIMS_INTTYPES_H

/* Block MSL's inttypes.h */
#define _MSL_INTTYPES_H
#define _INTTYPES_H
#define __INTTYPES_H__

#include "stdint.h"

/* printf format macros for signed types */
#ifndef PRId8
#define PRId8   "d"
#endif
#ifndef PRId16
#define PRId16  "d"
#endif
#ifndef PRId32
#define PRId32  "ld"
#endif
#ifndef PRId64
#define PRId64  "lld"
#endif

/* printf format macros for unsigned types */
#ifndef PRIu8
#define PRIu8   "u"
#endif
#ifndef PRIu16
#define PRIu16  "u"
#endif
#ifndef PRIu32
#define PRIu32  "lu"
#endif
#ifndef PRIu64
#define PRIu64  "llu"
#endif

/* printf format macros for hex */
#ifndef PRIx8
#define PRIx8   "x"
#endif
#ifndef PRIx16
#define PRIx16  "x"
#endif
#ifndef PRIx32
#define PRIx32  "lx"
#endif
#ifndef PRIx64
#define PRIx64  "llx"
#endif

/* size_t format macros (NetSurf-specific) */
#ifndef PRIsizet
#define PRIsizet "lu"
#endif
#ifndef PRIssizet
#define PRIssizet "ld"
#endif

#endif /* MACOS9_SHIMS_INTTYPES_H */
