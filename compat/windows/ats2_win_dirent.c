#if defined(_WIN32)

#include "ats2_win_dirent.h"
#include <windows.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

struct atslib_win_DIR {
    wchar_t *pattern;
    HANDLE handle;
    WIN32_FIND_DATAW pending;
    int has_pending;
    int eof;
    int failure;
    long position;
    struct dirent entry;
};

static int win_errno(DWORD error) {
    switch (error) {
        case ERROR_FILE_NOT_FOUND:
        case ERROR_PATH_NOT_FOUND: return ENOENT;
        case ERROR_ACCESS_DENIED: return EACCES;
        case ERROR_INVALID_NAME:
        case ERROR_INVALID_PARAMETER: return EINVAL;
        case ERROR_DIRECTORY: return ENOTDIR;
        case ERROR_NOT_ENOUGH_MEMORY:
        case ERROR_OUTOFMEMORY: return ENOMEM;
        case ERROR_FILENAME_EXCED_RANGE: return ENAMETOOLONG;
        default: return EIO;
    }
}

static wchar_t *to_utf16(const char *text) {
    if (!text || !*text) { errno = ENOENT; return NULL; }
    size_t bytes = strlen(text);
    if (bytes >= INT_MAX) { errno = ENAMETOOLONG; return NULL; }
    int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, NULL, 0);
    if (n <= 0) { errno = EILSEQ; return NULL; }
    if ((size_t)n > SIZE_MAX / sizeof(wchar_t)) { errno = ENOMEM; return NULL; }
    wchar_t *result = (wchar_t *)malloc((size_t)n * sizeof(wchar_t));
    if (!result) { errno = ENOMEM; return NULL; }
    if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text, -1, result, n)) {
        free(result);
        errno = EILSEQ;
        return NULL;
    }
    for (int i = 0; i < n; ++i) {
        if (result[i] == L'/') result[i] = L'\\';
    }
    return result;
}

static int restart(DIR *dir) {
    if (dir->handle != INVALID_HANDLE_VALUE) {
        FindClose(dir->handle);
        dir->handle = INVALID_HANDLE_VALUE;
    }
    dir->has_pending = 0;
    dir->eof = 0;
    dir->failure = 0;
    dir->position = 0;
    dir->handle = FindFirstFileW(dir->pattern, &dir->pending);
    if (dir->handle == INVALID_HANDLE_VALUE) {
        DWORD error = GetLastError();
        if (error == ERROR_FILE_NOT_FOUND || error == ERROR_NO_MORE_FILES) {
            dir->eof = 1;
            return 0;
        }
        dir->failure = win_errno(error);
        errno = dir->failure;
        return -1;
    }
    dir->has_pending = 1;
    return 0;
}

DIR *atslib_win_opendir(const char *path) {
    wchar_t *wide = to_utf16(path);
    if (!wide) return NULL;
    DWORD attributes = GetFileAttributesW(wide);
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        int error = win_errno(GetLastError());
        free(wide);
        errno = error;
        return NULL;
    }
    if (!(attributes & FILE_ATTRIBUTE_DIRECTORY)) {
        free(wide);
        errno = ENOTDIR;
        return NULL;
    }
    size_t count = wcslen(wide);
    if (count > (SIZE_MAX / sizeof(wchar_t)) - 3) {
        free(wide);
        errno = ENAMETOOLONG;
        return NULL;
    }
    DIR *dir = (DIR *)calloc(1, sizeof(*dir));
    if (!dir) {
        free(wide);
        errno = ENOMEM;
        return NULL;
    }
    dir->pattern = (wchar_t *)malloc((count + 3) * sizeof(wchar_t));
    if (!dir->pattern) {
        free(wide);
        free(dir);
        errno = ENOMEM;
        return NULL;
    }
    memcpy(dir->pattern, wide, count * sizeof(wchar_t));
    free(wide);
    size_t end = count;
    if (end > 0 && dir->pattern[end - 1] != L'\\') dir->pattern[end++] = L'\\';
    dir->pattern[end++] = L'*';
    dir->pattern[end] = L'\0';
    dir->handle = INVALID_HANDLE_VALUE;
    if (restart(dir) != 0) {
        int error = errno;
        free(dir->pattern);
        free(dir);
        errno = error;
        return NULL;
    }
    return dir;
}

static int read_next(DIR *dir) {
    if (dir->failure) { errno = dir->failure; return -1; }
    if (dir->eof) return 0;
    if (dir->position == LONG_MAX) { errno = EOVERFLOW; return -1; }
    WIN32_FIND_DATAW data;
    if (dir->has_pending) {
        data = dir->pending;
    } else {
        if (!FindNextFileW(dir->handle, &data)) {
            DWORD error = GetLastError();
            FindClose(dir->handle);
            dir->handle = INVALID_HANDLE_VALUE;
            if (error == ERROR_NO_MORE_FILES) {
                dir->eof = 1;
                return 0;
            }
            dir->failure = win_errno(error);
            errno = dir->failure;
            return -1;
        }
    }
    dir->has_pending = 0;
    int converted = WideCharToMultiByte(
        CP_UTF8, WC_ERR_INVALID_CHARS, data.cFileName, -1,
        dir->entry.d_name, sizeof(dir->entry.d_name), NULL, NULL
    );
    if (converted == 0) {
        DWORD error = GetLastError();
        errno = error == ERROR_INSUFFICIENT_BUFFER ? ENAMETOOLONG : EILSEQ;
        return -1;
    }
    dir->entry.d_ino = 0;
    dir->position += 1;
    return 1;
}

struct dirent *atslib_win_readdir(DIR *dir) {
    if (!dir) { errno = EBADF; return NULL; }
    int read = read_next(dir);
    return read == 1 ? &dir->entry : NULL;
}

int atslib_win_readdir_r(DIR *dir, struct dirent *entry, struct dirent **result) {
    if (!result) { errno = EINVAL; return EINVAL; }
    *result = NULL;
    if (!dir || !entry) { errno = EINVAL; return EINVAL; }
    int read = read_next(dir);
    if (read == -1) return errno;
    if (read == 0) return 0;
    *entry = dir->entry;
    *result = entry;
    return 0;
}

int atslib_win_closedir(DIR *dir) {
    if (!dir) { errno = EBADF; return -1; }
    BOOL ok = TRUE;
    DWORD error = 0;
    if (dir->handle != INVALID_HANDLE_VALUE) {
        ok = FindClose(dir->handle);
        if (!ok) error = GetLastError();
    }
    free(dir->pattern);
    free(dir);
    if (!ok) { errno = win_errno(error); return -1; }
    return 0;
}

void atslib_win_rewinddir(DIR *dir) {
    if (!dir) { errno = EBADF; return; }
    restart(dir);
}

long atslib_win_telldir(DIR *dir) {
    if (!dir) { errno = EBADF; return -1; }
    return dir->position;
}

void atslib_win_seekdir(DIR *dir, long offset) {
    if (!dir) { errno = EBADF; return; }
    if (offset < 0) { errno = EINVAL; return; }
    if (restart(dir) != 0) return;
    while (dir->position < offset) {
        int read = read_next(dir);
        if (read <= 0) return;
    }
}

#endif
