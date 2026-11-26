#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winioctl.h>
#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#ifndef SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE
#define SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE 0x2
#endif

static int ats2_win_set_error(DWORD err) {
    int value;
    switch (err) {
        case ERROR_FILE_NOT_FOUND:
        case ERROR_PATH_NOT_FOUND:
            value = ENOENT; break;
        case ERROR_ALREADY_EXISTS:
        case ERROR_FILE_EXISTS:
            value = EEXIST; break;
        case ERROR_ACCESS_DENIED:
        case ERROR_SHARING_VIOLATION:
            value = EACCES; break;
        case ERROR_PRIVILEGE_NOT_HELD:
            value = EPERM; break;
        case ERROR_NOT_SAME_DEVICE:
            value = EXDEV; break;
        case ERROR_NOT_ENOUGH_MEMORY:
        case ERROR_OUTOFMEMORY:
            value = ENOMEM; break;
        case ERROR_INSUFFICIENT_BUFFER:
        case ERROR_BUFFER_OVERFLOW:
            value = ERANGE; break;
        case ERROR_INVALID_PARAMETER:
        case ERROR_INVALID_NAME:
        case ERROR_NO_UNICODE_TRANSLATION:
        case ERROR_NOT_A_REPARSE_POINT:
            value = EINVAL; break;
        case ERROR_NOT_SUPPORTED:
        case ERROR_INVALID_FUNCTION:
            value = ENOTSUP; break;
        default:
            value = EIO; break;
    }
    errno = value;
    return value;
}

static wchar_t *ats2_win_to_wide(const char *utf8) {
    if (utf8 == NULL) { errno = EINVAL; return NULL; }
    int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8, -1, NULL, 0);
    if (n <= 0) { ats2_win_set_error(GetLastError()); return NULL; }
    wchar_t *out = (wchar_t *)malloc((size_t)n * sizeof(wchar_t));
    if (out == NULL) { errno = ENOMEM; return NULL; }
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8, -1, out, n) != n) {
        DWORD e = GetLastError();
        free(out);
        ats2_win_set_error(e);
        return NULL;
    }
    return out;
}

int atslib_libats_win_getlogin_r(char *buf, size_t size) {
    if (buf == NULL || size == 0) { errno = ERANGE; return ERANGE; }
    DWORD cap = 64;
    for (;;) {
        wchar_t *name = (wchar_t *)malloc((size_t)cap * sizeof(wchar_t));
        if (name == NULL) { errno = ENOMEM; return ENOMEM; }
        DWORD count = cap;
        if (GetUserNameW(name, &count)) {
            int needed = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, name, -1, NULL, 0, NULL, NULL);
            if (needed <= 0) {
                DWORD e = GetLastError();
                free(name);
                return ats2_win_set_error(e);
            }
            if ((size_t)needed > size) { free(name); errno = ERANGE; return ERANGE; }
            if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, name, -1, buf, needed, NULL, NULL) != needed) {
                DWORD e = GetLastError();
                free(name);
                return ats2_win_set_error(e);
            }
            free(name);
            return 0;
        }
        DWORD e = GetLastError();
        free(name);
        if (e != ERROR_INSUFFICIENT_BUFFER) return ats2_win_set_error(e);
        if (count > 32768U) { errno = ERANGE; return ERANGE; }
        cap = count > cap ? count : cap * 2;
        if (cap > 32768U) { errno = ERANGE; return ERANGE; }
    }
}

int atslib_libats_win_link(const char *existing, const char *new_name) {
    wchar_t *old_w = ats2_win_to_wide(existing);
    if (old_w == NULL) return -1;
    wchar_t *new_w = ats2_win_to_wide(new_name);
    if (new_w == NULL) { free(old_w); return -1; }
    BOOL ok = CreateHardLinkW(new_w, old_w, NULL);
    DWORD e = ok ? ERROR_SUCCESS : GetLastError();
    free(new_w);
    free(old_w);
    if (!ok) { ats2_win_set_error(e); return -1; }
    return 0;
}

static int ats2_win_target_is_dir(const wchar_t *target, const wchar_t *link_name) {
    if (target[0] != L'/' && target[0] != L'\\' && !(target[0] && target[1] == L':')) {
        const wchar_t *sep = wcsrchr(link_name, L'\\');
        const wchar_t *sep2 = wcsrchr(link_name, L'/');
        if (sep2 != NULL && (sep == NULL || sep2 > sep)) sep = sep2;
        if (sep != NULL) {
            size_t parent = (size_t)(sep - link_name) + 1;
            size_t target_len = wcslen(target);
            if (parent <= SIZE_MAX / sizeof(wchar_t) - target_len - 1) {
                wchar_t *joined = (wchar_t *)malloc((parent + target_len + 1) * sizeof(wchar_t));
                if (joined != NULL) {
                    memcpy(joined, link_name, parent * sizeof(wchar_t));
                    memcpy(joined + parent, target, (target_len + 1) * sizeof(wchar_t));
                    DWORD attr = GetFileAttributesW(joined);
                    free(joined);
                    if (attr != INVALID_FILE_ATTRIBUTES) return (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
                }
            }
        }
    }
    DWORD attr = GetFileAttributesW(target);
    return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

int atslib_libats_win_symlink(const char *target, const char *link_name) {
    wchar_t *target_w = ats2_win_to_wide(target);
    if (target_w == NULL) return -1;
    wchar_t *link_w = ats2_win_to_wide(link_name);
    if (link_w == NULL) { free(target_w); return -1; }
    DWORD flags = ats2_win_target_is_dir(target_w, link_w) ? SYMBOLIC_LINK_FLAG_DIRECTORY : 0;
    BOOL ok = CreateSymbolicLinkW(link_w, target_w, flags | SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE);
    DWORD e = ok ? ERROR_SUCCESS : GetLastError();
    if (!ok && e == ERROR_INVALID_PARAMETER) {
        ok = CreateSymbolicLinkW(link_w, target_w, flags);
        e = ok ? ERROR_SUCCESS : GetLastError();
    }
    free(link_w);
    free(target_w);
    if (!ok) { ats2_win_set_error(e); return -1; }
    return 0;
}

typedef struct {
    DWORD ReparseTag;
    WORD ReparseDataLength;
    WORD Reserved;
    WORD SubstituteNameOffset;
    WORD SubstituteNameLength;
    WORD PrintNameOffset;
    WORD PrintNameLength;
    DWORD Flags;
    WCHAR PathBuffer[1];
} ats2_win_symlink_reparse;

intptr_t atslib_libats_win_readlink(const char *path, void *buffer, size_t size) {
    if (buffer == NULL || size == 0 || size > INT_MAX) { errno = EINVAL; return -1; }
    wchar_t *path_w = ats2_win_to_wide(path);
    if (path_w == NULL) return -1;
    HANDLE handle = CreateFileW(path_w, 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                NULL, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, NULL);
    DWORD e = handle == INVALID_HANDLE_VALUE ? GetLastError() : ERROR_SUCCESS;
    free(path_w);
    if (handle == INVALID_HANDLE_VALUE) { ats2_win_set_error(e); return -1; }
    union { uint64_t align; unsigned char bytes[16384]; } storage;
    DWORD used = 0;
    BOOL ok = DeviceIoControl(handle, FSCTL_GET_REPARSE_POINT, NULL, 0,
                              storage.bytes, (DWORD)sizeof(storage.bytes), &used, NULL);
    e = ok ? ERROR_SUCCESS : GetLastError();
    CloseHandle(handle);
    if (!ok) { ats2_win_set_error(e); return -1; }
    const ats2_win_symlink_reparse *point = (const ats2_win_symlink_reparse *)storage.bytes;
    size_t header = offsetof(ats2_win_symlink_reparse, PathBuffer);
    if (used < header || point->ReparseTag != IO_REPARSE_TAG_SYMLINK) {
        errno = EINVAL;
        return -1;
    }
    size_t offset = point->PrintNameOffset;
    size_t length = point->PrintNameLength;
    size_t fixed = header - 8;
    size_t available = (size_t)point->ReparseDataLength;
    if ((offset | length) & 1 || offset > used - header || length > used - header - offset ||
        available < fixed || offset > available - fixed || length > available - fixed - offset) {
        errno = EINVAL;
        return -1;
    }
    const wchar_t *name = (const wchar_t *)((const unsigned char *)point->PathBuffer + offset);
    int wide_len = (int)(length / sizeof(wchar_t));
    if (wide_len == 0) return 0;
    int utf8_len = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, name, wide_len, NULL, 0, NULL, NULL);
    if (utf8_len <= 0) { ats2_win_set_error(GetLastError()); return -1; }
    char *utf8 = (char *)malloc((size_t)utf8_len);
    if (utf8 == NULL) { errno = ENOMEM; return -1; }
    if (WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, name, wide_len, utf8, utf8_len, NULL, NULL) != utf8_len) {
        e = GetLastError(); free(utf8); ats2_win_set_error(e); return -1;
    }
    size_t copied = (size_t)utf8_len < size ? (size_t)utf8_len : size;
    memcpy(buffer, utf8, copied);
    free(utf8);
    return (intptr_t)copied;
}
#endif
