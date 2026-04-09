/*
 * MacSurf stub -- shims/stat.h
 * Pulls in mac_types.h for POSIX type definitions (mode_t, off_t,
 * ssize_t) and forward declarations for mac_stat/mac_fstat/mac_access.
 *
 * struct stat, S_IFDIR, S_IFREG, S_ISDIR, S_ISREG, S_I*USR, and
 * O_* constants are all provided by MSL's stat.h and fcntl.h —
 * we intentionally do NOT redefine them here.
 *
 * C89-compatible.  Licensed under GPL v2.
 */

#ifndef MACOS9_SHIMS_STAT_H
#define MACOS9_SHIMS_STAT_H

#include "mac_types.h"

#endif /* MACOS9_SHIMS_STAT_H */
