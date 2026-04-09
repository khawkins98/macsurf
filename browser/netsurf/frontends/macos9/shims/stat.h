/*
 * MacSurf stub -- shims/stat.h
 * Provides struct stat, stat-related macros, and POSIX type definitions.
 *
 * This shim is found before MSL's stat.h on the system search path,
 * so we must provide everything that MSL's stat.h would.
 *
 * C89-compatible.  Licensed under GPL v2.
 */

#ifndef MACOS9_SHIMS_STAT_H
#define MACOS9_SHIMS_STAT_H

#include "mac_types.h"

/* --- struct stat --- */
#ifndef _STAT_H_STRUCT
#define _STAT_H_STRUCT
struct stat {
	unsigned long	st_dev;
	unsigned long	st_ino;
	unsigned short	st_mode;
	unsigned short	st_nlink;
	unsigned short	st_uid;
	unsigned short	st_gid;
	unsigned long	st_rdev;
	long		st_size;
	long		st_atime;
	long		st_mtime;
	long		st_ctime;
	long		st_blksize;
	long		st_blocks;
};
#endif

/* --- stat macros --- */
#ifndef S_IFDIR
#define S_IFDIR   0040000
#endif
#ifndef S_IFREG
#define S_IFREG   0100000
#endif
#ifndef S_ISDIR
#define S_ISDIR(m)  (((m) & 0170000) == S_IFDIR)
#endif
#ifndef S_ISREG
#define S_ISREG(m)  (((m) & 0170000) == S_IFREG)
#endif
#ifndef S_IRWXU
#define S_IRWXU   0000700
#endif
#ifndef S_IRUSR
#define S_IRUSR   0000400
#endif
#ifndef S_IWUSR
#define S_IWUSR   0000200
#endif
#ifndef S_IXUSR
#define S_IXUSR   0000100
#endif

/* --- function declarations --- */
extern int stat(const char *path, struct stat *buf);
extern int fstat(int fd, struct stat *buf);

#endif /* MACOS9_SHIMS_STAT_H */
