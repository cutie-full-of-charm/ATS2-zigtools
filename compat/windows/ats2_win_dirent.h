#ifndef ATS2_WIN_DIRENT_H
#define ATS2_WIN_DIRENT_H

#if defined(_WIN32)

#include <sys/types.h>
#include <stddef.h>

#define ATS2_WIN_DIRENT_NAME_MAX 1024

typedef struct atslib_win_DIR DIR;

struct dirent {
    ino_t d_ino;
    char d_name[ATS2_WIN_DIRENT_NAME_MAX + 1];
};

DIR *atslib_win_opendir(const char *path);
int atslib_win_closedir(DIR *dir);
struct dirent *atslib_win_readdir(DIR *dir);
int atslib_win_readdir_r(DIR *dir, struct dirent *entry, struct dirent **result);
void atslib_win_rewinddir(DIR *dir);
long atslib_win_telldir(DIR *dir);
void atslib_win_seekdir(DIR *dir, long offset);

#endif
#endif
