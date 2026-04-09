#ifndef MACSURF_DIRENT_H
#define MACSURF_DIRENT_H

typedef void DIR;
struct dirent {
    char d_name[256];
};

DIR *opendir(const char *name);
struct dirent *readdir(DIR *dirp);
int closedir(DIR *dirp);
int alphasort(const struct dirent **d1, const struct dirent **d2);
int scandir(const char *dirp, struct dirent ***namelist,
  int (*filter)(const struct dirent *),
  int (*compar)(const struct dirent **, const struct dirent **));

#endif
