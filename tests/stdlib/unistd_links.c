#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "share/H/pats_atslib.h"
#include "libats/libc/CATS/unistd.cats"

static int fail(const char *message) {
    fprintf(stderr, "unistd links failed: %s (errno=%d)\n", message, errno);
    return 1;
}

int main(void) {
    const char *original = "ats2_unistd_links_source.tmp";
    const char *hardlink = "ats2_unistd_links_hard.tmp";
    const char *softlink = "ats2_unistd_links_soft.tmp";
    const char contents[] = "ats2-windows-links";
    char text[64];
    FILE *file = fopen(original, "wb");
    if (file == NULL) return fail("create source");
    if (fwrite(contents, 1, sizeof contents, file) != sizeof contents) return fail("write source");
    if (fclose(file) != 0) return fail("close source");

    if (atslib_libats_libc_link(original, hardlink) != 0) return fail("create hardlink");
    file = fopen(hardlink, "rb");
    if (file == NULL) return fail("open hardlink");
    if (fread(text, 1, sizeof contents, file) != sizeof contents) return fail("read hardlink");
    if (fclose(file) != 0) return fail("close hardlink");
    if (memcmp(text, contents, sizeof contents) != 0) return fail("hardlink contents");
    errno = 0;
    if (atslib_libats_libc_link(original, hardlink) != -1 || errno != EEXIST) return fail("hardlink exists");

    errno = 0;
    if (atslib_libats_libc_symlink(original, softlink) == 0) {
        char buffer[256];
        intptr_t n = (intptr_t)atslib_libats_libc_readlink(softlink, buffer, sizeof buffer);
        if (n < 0 || (size_t)n != strlen(original)) return fail("readlink length");
        if (memcmp(buffer, original, (size_t)n) != 0) return fail("readlink contents");
        if (atslib_libats_libc_unlink(softlink) != 0) return fail("unlink symlink");
    } else {
#if defined(_WIN32)
        if (errno != EPERM && errno != EACCES && errno != ENOTSUP) return fail("symlink");
        puts("unistd symlink skipped: Windows privilege or filesystem restriction");
#else
        return fail("symlink");
#endif
    }

#if defined(_WIN32)
    {
        char name[256];
        if (atslib_libats_libc_getlogin_r(name, sizeof name) != 0 || name[0] == '\0') return fail("getlogin_r");
        char tiny[1];
        errno = 0;
        if (atslib_libats_libc_getlogin_r(tiny, sizeof tiny) != ERANGE || errno != ERANGE)
            return fail("getlogin_r ERANGE");
    }
#endif
    if (atslib_libats_libc_unlink(hardlink) != 0) return fail("unlink hardlink");
    if (atslib_libats_libc_unlink(original) != 0) return fail("unlink original");
    puts("unistd links ok");
    return 0;
}
