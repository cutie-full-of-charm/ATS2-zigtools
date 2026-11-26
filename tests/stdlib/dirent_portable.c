#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pats_ccomp_basics.h"
#include "pats_ccomp_typedefs.h"
#include "share/H/pats_atslib.h"
#include "libats/libc/CATS/dirent.cats"

#if defined(_WIN32)
#include <windows.h>
#include <wchar.h>

static int fail_win32(const char *what) {
    fprintf(stderr, "dirent portable failed: %s (GetLastError=%lu)\n", what, (unsigned long)GetLastError());
    return 1;
}

static wchar_t *extended_absolute_path(const wchar_t *relative) {
    DWORD needed = GetFullPathNameW(relative, 0, NULL, NULL);
    if (needed == 0) return NULL;
    if (needed > 32760) { SetLastError(ERROR_FILENAME_EXCED_RANGE); return NULL; }
    wchar_t *full = (wchar_t *)malloc((size_t)needed * sizeof(wchar_t));
    if (!full) { SetLastError(ERROR_OUTOFMEMORY); return NULL; }
    DWORD len = GetFullPathNameW(relative, needed, full, NULL);
    if (len == 0 || len >= needed) {
        DWORD error = GetLastError();
        free(full);
        SetLastError(error ? error : ERROR_INSUFFICIENT_BUFFER);
        return NULL;
    }
    int unc = len >= 2 && full[0] == L'\\' && full[1] == L'\\';
    const wchar_t *prefix = unc ? L"\\\\?\\UNC\\" : L"\\\\?\\";
    size_t prefix_len = wcslen(prefix);
    size_t skip = unc ? 2 : 0;
    size_t count = prefix_len + (size_t)len - skip + 1;
    wchar_t *out = (wchar_t *)malloc(count * sizeof(wchar_t));
    if (!out) {
        free(full);
        SetLastError(ERROR_OUTOFMEMORY);
        return NULL;
    }
    wmemcpy(out, prefix, prefix_len);
    wmemcpy(out + prefix_len, full + skip, (size_t)len - skip + 1);
    free(full);
    return out;
}
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

static int fail(const char *what) {
    fprintf(stderr, "dirent portable failed: %s (errno=%d)\n", what, errno);
    return 1;
}

static int make_dir(const char *path) {
#if defined(_WIN32)
    return CreateDirectoryA(path, NULL) ? 0 : -1;
#else
    return mkdir(path, 0700);
#endif
}

static int remove_dir(const char *path) {
#if defined(_WIN32)
    return RemoveDirectoryA(path) ? 0 : -1;
#else
    return rmdir(path);
#endif
}

int main(void) {
    const char *dirpath = "ats2_dirent_stage4_tmp";
    const char *file_a = "ats2_dirent_stage4_tmp/alpha.txt";
    const char *file_b = "ats2_dirent_stage4_tmp/beta.txt";
    if (make_dir(dirpath) != 0) return fail("mkdir");
    FILE *file = fopen(file_a, "wb");
    if (!file) return fail("fopen alpha");
    if (fclose(file) != 0) return fail("close alpha");
    file = fopen(file_b, "wb");
    if (!file) return fail("fopen beta");
    if (fclose(file) != 0) return fail("close beta");

#if defined(_WIN32)
    wchar_t long_path[320];
    const wchar_t wide_prefix[] = L"ats2_dirent_stage4_tmp\\";
    size_t prefix_size = wcslen(wide_prefix);
    memcpy(long_path, wide_prefix, prefix_size * sizeof(wchar_t));
    for (int i = 0; i < 220; ++i) long_path[prefix_size + (size_t)i] = L'\x4e2d';
    long_path[prefix_size + 220] = 0;
    wchar_t *unicode_path = extended_absolute_path(long_path);
    if (!unicode_path) return fail_win32("resolve unicode file path");
    HANDLE wide_file = CreateFileW(unicode_path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
    if (wide_file == INVALID_HANDLE_VALUE) return fail_win32("create unicode file");
    if (!CloseHandle(wide_file)) return fail_win32("close unicode file");
    char unicode_name[700];
    for (int i = 0; i < 220; ++i) memcpy(unicode_name + 3*i, "\xe4\xb8\xad", 3);
    unicode_name[660] = 0;
#endif

    DIR *dir = atslib_libats_libc_opendir(dirpath);
    if (!dir) return fail("opendir");
    int seen_a = 0, seen_b = 0;
#if defined(_WIN32)
    int seen_unicode = 0;
#endif
    int entries = 0;
    struct dirent entry;
    struct dirent *result;
    for (;;) {
        result = NULL;
        int rc = atslib_libats_libc_readdir_r(dir, &entry, &result);
        if (rc != 0) return fail("readdir_r");
        if (!result) break;
        ++entries;
        if (!strcmp(entry.d_name, "alpha.txt")) ++seen_a;
        if (!strcmp(entry.d_name, "beta.txt")) ++seen_b;
#if defined(_WIN32)
        if (!strcmp(entry.d_name, unicode_name)) ++seen_unicode;
#endif
        if (entries > 128) return fail("unexpected number of entries");
    }
    if (seen_a != 1 || seen_b != 1) return fail("missing or repeated entry");
#if defined(_WIN32)
    if (seen_unicode != 1) return fail("unicode entry not preserved");
#endif
    atslib_libats_libc_rewinddir(dir);
    errno = 0;
    if (!atslib_libats_libc_readdir(dir)) return fail("rewinddir / readdir");
    long offset = atslib_libats_libc_telldir(dir);
    struct dirent *next = atslib_libats_libc_readdir(dir);
    if (next) {
        char saved[sizeof(entry.d_name)];
        strcpy(saved, next->d_name);
        atslib_libats_libc_seekdir(dir, offset);
        next = atslib_libats_libc_readdir(dir);
        if (!next || strcmp(saved, next->d_name)) return fail("seekdir / telldir");
    }
    if (atslib_libats_libc_closedir(dir) != 0) return fail("closedir");
    errno = 0;
    if (atslib_libats_libc_opendir("ats2_dirent_stage4_does_not_exist")) return fail("missing path");
    if (errno != ENOENT) return fail("missing path errno");
    errno = 0;
    if (atslib_libats_libc_opendir(file_a)) return fail("regular file as dir");
    if (errno != ENOTDIR) return fail("ENOTDIR");
#if defined(_WIN32)
    if (!DeleteFileW(unicode_path)) return fail_win32("delete unicode file");
    free(unicode_path);
#endif
    if (remove(file_b) != 0 || remove(file_a) != 0) return fail("remove files");
    if (remove_dir(dirpath) != 0) return fail("rmdir");
    puts("dirent portable ok");
    return 0;
}
