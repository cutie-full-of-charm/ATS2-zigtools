#include <windows.h>
#include <winioctl.h>
#include <direct.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include "ats_memory.h"

ats_ptr_type atslib_getcwd0(void) {
    int size = 256;
    for (;;) {
        char *buf = (char *)ats_malloc_gc((size_t)size);
        if (!buf) {
            fputs("ATS2: out of memory\n", stderr);
            exit(1);
        }
        if (_getcwd(buf, size) != NULL) {
            /* Windows path to forward-slash path for ATS2 */
            for (char *p = buf; *p != '\0'; ++p) {
                if (*p == '\\') {
                    *p = '/';
                }
            }
            return (ats_ptr_type)buf;
        }
        int err = errno;
        ats_free_gc(buf);
        if (err != ERANGE || size > INT_MAX / 2) {
            errno = err;
            perror("ATS2: _getcwd");
            exit(1);
        }
        size *= 2;
    }
}

int atslib_win_test_file_islnk(const char *path) {
    WIN32_FIND_DATAA data;
    HANDLE handle = FindFirstFileA(path, &data);
    if (handle == INVALID_HANDLE_VALUE) return -1;
    int is_link = (data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) &&
                  (data.dwReserved0 == IO_REPARSE_TAG_SYMLINK);
    FindClose(handle);
    return is_link ? 1 : 0;
}
