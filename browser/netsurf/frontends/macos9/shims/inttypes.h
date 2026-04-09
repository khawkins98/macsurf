#ifndef MACSURF_INTTYPES_H
#define MACSURF_INTTYPES_H

/* Do NOT #include <stdint.h> — the prefix file provides types globally.
 * MSL's stdint.h pulls in C++ cstdint which breaks the C89 build. */

/* printf format macros */
#define PRId32 "ld"
#define PRIu32 "lu"
#define PRIx32 "lx"
#define PRId64 "lld"
#define PRIu64 "llu"
#define PRIx64 "llx"
#define PRIsizet "u"

/* scanf format macros (needed by nsoption.c) */
#define SCNx32 "lx"
#define SCNd32 "ld"
#define SCNu32 "lu"

#endif
