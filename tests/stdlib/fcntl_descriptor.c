#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "pats_ccomp_basics.h"
#include "pats_ccomp_typedefs.h"
#include "share/H/pats_atslib.h"
#include "libats/libc/CATS/fcntl.cats"
#include "libats/libc/CATS/unistd.cats"

extern int atslib_libats_libc_fildes_iget_int(int);

static int fail(const char *msg) {
    fprintf(stderr, "fcntl descriptor failed: %s (errno=%d)\n", msg, errno);
    return 1;
}

int main(void) {
    const char *path = "ats2_fcntl_descriptor_smoke.tmp";
    const char data[] = { 'x', '\n', 'y' };
    int fd = atslib_libats_libc_open_flags_mode(path, O_CREAT | O_EXCL | O_RDWR, 0600);
    if (fd < 0) return fail("open");
    if (atslib_libats_libc_fildes_iget_int(fd) != fd) return fail("live descriptor");
    if (atslib_libats_libc_write_err(fd, data, sizeof data) != (intptr_t)sizeof data) return fail("write");
    char check[sizeof data];
#ifdef _WIN32
    if (_lseek(fd, 0, SEEK_SET) < 0) return fail("seek");
#else
    if (lseek(fd, 0, SEEK_SET) < 0) return fail("seek");
#endif
    if (atslib_libats_libc_read_err(fd, check, sizeof check) != (intptr_t)sizeof check) return fail("read");
    if (memcmp(data, check, sizeof data) != 0) return fail("binary data");

#ifdef _WIN32
    if (atslib_libats_win_dup2_fildes(fd, fd) != -1 || errno != EINVAL) return fail("occupied target");
    int dupfd = fd + 32;
    if (atslib_libats_win_dup2_fildes(fd, dupfd) != dupfd) return fail("unused target");
    if (atslib_libats_libc_fildes_iget_int(dupfd) != dupfd) return fail("duplicated descriptor");
    if (atslib_libats_libc_close(dupfd) != 0) return fail("close duplicate");
#endif

    if (atslib_libats_libc_close(fd) != 0) return fail("close");
    if (atslib_libats_libc_fildes_iget_int(fd) != -1) return fail("closed descriptor");
    errno = 0;
    if (atslib_libats_libc_fildes_iget_int(-1) != -1 || errno != EBADF) return fail("negative descriptor");
    if (remove(path) != 0) return fail("remove");
    puts("fcntl descriptor ok");
    return 0;
}
