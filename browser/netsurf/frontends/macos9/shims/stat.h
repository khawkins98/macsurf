#ifndef MACSURF_STAT_H
#define MACSURF_STAT_H

/* time_t and mode_t are already provided globally by macsurf_prefix.h */

typedef unsigned long dev_t;
typedef unsigned long ino_t;
typedef short nlink_t;
typedef unsigned long uid_t;
typedef unsigned long gid_t;
typedef long off_t;

struct stat {
    dev_t st_dev;
    ino_t st_ino;
    mode_t st_mode;
    nlink_t st_nlink;
    uid_t st_uid;
    gid_t st_gid;
    dev_t st_rdev;
    off_t st_size;
    time_t st_atime;
    time_t st_mtime;
    time_t st_ctime;
    long st_blksize;
    long st_blocks;
};

#define S_IFMT   0170000
#define S_IFDIR  0040000
#define S_IFCHR  0020000
#define S_IFBLK  0060000
#define S_IFREG  0100000
#define S_IFIFO  0010000
#define S_IFLNK  0120000
#define S_IFSOCK 0140000

#define S_ISDIR(m)      (((m) & S_IFMT) == S_IFDIR)
#define S_ISREG(m)      (((m) & S_IFMT) == S_IFREG)

int stat(const char *path, struct stat *buf);
int mkdir(const char *path, mode_t mode);

#endif
