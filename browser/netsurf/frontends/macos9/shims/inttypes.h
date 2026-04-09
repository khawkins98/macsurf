#ifndef MACSURF_INTTYPES_H
#define MACSURF_INTTYPES_H

/* Do NOT #include <stdint.h> — the prefix file provides types globally.
 * MSL's stdint.h pulls in C++ cstdint which breaks the C89 build. */

/* printf format macros — guarded to avoid redefinition */
#ifndef PRId16
#define PRId16 "d"
#endif
#ifndef PRIu16
#define PRIu16 "u"
#endif
#ifndef PRId32
#define PRId32 "ld"
#endif
#ifndef PRIu32
#define PRIu32 "lu"
#endif
#ifndef PRIx32
#define PRIx32 "lx"
#endif
#ifndef PRIX32
#define PRIX32 "lX"
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
#ifndef PRIxPTR
#define PRIxPTR "lx"
#endif

/* size_t/ssize_t format macros — CW8 PPC uses unsigned long for size_t */
#ifndef PRIsizet
#define PRIsizet "lu"
#endif
#ifndef PRIssizet
#define PRIssizet "ld"
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
