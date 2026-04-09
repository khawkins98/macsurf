#ifndef MACSURF_INTTYPES_H
#define MACSURF_INTTYPES_H

/* Do NOT #include <stdint.h> — the prefix file provides types globally.
 * MSL's stdint.h pulls in C++ cstdint which breaks the C89 build. */

/* printf format macros — guarded to avoid redefinition */
#ifndef PRId32
#define PRId32 "ld"
#endif
#ifndef PRIu32
#define PRIu32 "lu"
#endif
#ifndef PRIx32
#define PRIx32 "lx"
#endif
#ifndef PRId64
#define PRId64 "lld"
#endif
#ifndef PRIu64
#define PRIu64 "llu"
#endif
#ifndef PRIx64
#define PRIx64 "llx"
#endif
#ifndef PRIsizet
#define PRIsizet "zu"
#endif
#ifndef PRIssizet
#define PRIssizet "zd"
#endif

/* scanf format macros (needed by nsoption.c) */
#ifndef SCNx32
#define SCNx32 "lx"
#endif
#ifndef SCNd32
#define SCNd32 "ld"
#endif
#ifndef SCNu32
#define SCNu32 "lu"
#endif

#endif
