#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#include <io.h>
#include "ats2_win_crt_invalid_parameter.h"
#define fd_open(p) _open(p, _O_CREAT | _O_EXCL | _O_RDWR | _O_BINARY, 0600)
#define fd_close _close
#define fd_dup _dup
static int handler_calls;
static void __cdecl sentinel(const wchar_t *a, const wchar_t *b,
    const wchar_t *c, unsigned int d, uintptr_t e) {
    (void)a; (void)b; (void)c; (void)d; (void)e;
    ++handler_calls;
}
#else
#include <unistd.h>
#define fd_open(p) open(p, O_CREAT | O_EXCL | O_RDWR, 0600)
#define fd_close close
#define fd_dup dup
#endif
extern int atslib_libats_libc_fildes_iget_int(int);
extern int atslib_libats_libc_dup2_fildes(int, int);
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "fd guard failed line %d errno=%d\n", __LINE__, errno); return 1; } } while (0)
int main(void) {
#ifdef _WIN32
    _invalid_parameter_handler old = _set_thread_local_invalid_parameter_handler(sentinel);
#endif
    const char *path = "ats2_fd_guard.tmp";
    int fd = fd_open(path);
    CHECK(fd >= 0);
    int dst = fd_dup(fd);
    CHECK(dst >= 0 && dst != fd);
    CHECK(atslib_libats_libc_fildes_iget_int(fd) == fd);
    CHECK(atslib_libats_libc_fildes_iget_int(-1) == -1 && errno == EBADF);
    CHECK(atslib_libats_libc_fildes_iget_int(INT_MAX) == -1 && errno == EBADF);
    CHECK(atslib_libats_libc_dup2_fildes(fd, dst) == -1 && errno == EINVAL);
    CHECK(atslib_libats_libc_dup2_fildes(fd, fd) == -1 && errno == EINVAL);
    CHECK(fd_close(dst) == 0);
    CHECK(atslib_libats_libc_fildes_iget_int(dst) == -1 && errno == EBADF);
    CHECK(atslib_libats_libc_dup2_fildes(fd, dst) == dst);
    CHECK(atslib_libats_libc_fildes_iget_int(dst) == dst);
    CHECK(atslib_libats_libc_dup2_fildes(fd, INT_MAX) == -1);
    CHECK(atslib_libats_libc_fildes_iget_int(fd) == fd);
    CHECK(fd_close(dst) == 0 && fd_close(fd) == 0);
    CHECK(remove(path) == 0);
#ifdef _WIN32
    CHECK(_set_thread_local_invalid_parameter_handler(old) == sentinel);
    CHECK(handler_calls == 0);
#endif
    puts("fd guard ok");
    return 0;
}
