#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "libats/libc/CATS/unistd.cats"

#ifdef _WIN32
#include <io.h>
#define test_open(path) _open((path), _O_CREAT | _O_TRUNC | _O_RDWR | _O_BINARY, _S_IREAD | _S_IWRITE)
#define test_seek(fd) _lseek((fd), 0L, SEEK_SET)
#else
#define test_open(path) open((path), O_CREAT | O_TRUNC | O_RDWR, 0600)
#define test_seek(fd) lseek((fd), 0, SEEK_SET)
#endif

static int fail(const char *what) {
    fprintf(stderr, "unistd portable failed: %s (errno=%d)\n", what, errno);
    return 1;
}

int main(void) {
    const char *path = "ats2_unistd_portable_smoke.tmp";
    const char payload[] = "portable descriptor data";
    char readbuf[sizeof(payload)] = {0};
    char cwd[4096];
    int fd = -1;
    int fd2 = -1;
    int result = 1;

    if (atslib_libats_libc_getcwd(cwd, sizeof(cwd)) == NULL) return fail("getcwd");
    if (atslib_libats_libc_chdir(".") != 0) return fail("chdir");
    if (atslib_libats_libc_getpid() <= 0) return fail("getpid");
#ifdef _WIN32
    if (strchr(cwd, '\\') != NULL) return fail("cwd normalization");
#endif

    fd = test_open(path);
    if (fd < 0) return fail("open");

    if (atslib_libats_libc_write_err(fd, payload, sizeof(payload)) != (intptr_t)sizeof(payload)) {
        (void)atslib_libats_libc_close(fd);
        (void)remove(path);
        return fail("write");
    }
    fd2 = atslib_libats_libc_dup(fd);
    if (fd2 < 0) goto cleanup;
    if (atslib_libats_libc_dup2(fd, fd2) != fd2) goto cleanup;
    if (atslib_libats_libc_close(fd) != 0) goto cleanup;
    fd = -1;
    if (test_seek(fd2) < 0) goto cleanup;
    if (atslib_libats_libc_read_err(fd2, readbuf, sizeof(readbuf)) != (intptr_t)sizeof(readbuf)) goto cleanup;
    if (memcmp(payload, readbuf, sizeof(payload)) != 0) goto cleanup;
    if (atslib_libats_libc_read_err(fd2, readbuf, 1) != 0) goto cleanup;
    if (atslib_libats_libc_close(fd2) != 0) goto cleanup;
    fd2 = -1;

#ifdef _WIN32
    errno = 0;
    if (atslib_libats_libc_close(-1) != -1 || errno != EBADF) goto cleanup;
    errno = 0;
    if (atslib_libats_libc_read_err(-1, readbuf, 1) != -1 || errno != EBADF) goto cleanup;
    errno = 0;
    if (atslib_libats_libc_write_err(-1, payload, 1) != -1 || errno != EBADF) goto cleanup;
    errno = 0;
    if (atslib_libats_libc_write_err(1, payload, (size_t)INT_MAX + 1) != -1 || errno != EINVAL) goto cleanup;
#endif
    result = 0;
cleanup:
    if (fd2 >= 0) (void)atslib_libats_libc_close(fd2);
    if (fd >= 0) (void)atslib_libats_libc_close(fd);
    if (remove(path) != 0) result = 1;
    if (result != 0) return fail("descriptor operations");
    puts("unistd portable ok");
    return 0;
}
