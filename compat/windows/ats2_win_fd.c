#if defined(_WIN32)
#include <errno.h>
#include <io.h>
#include <stdint.h>
#include <stdlib.h>
#include <windows.h>
#include "ats2_win_crt_invalid_parameter.h"

static void __cdecl atslib_fd_invalid_parameter(
    const wchar_t *expression, const wchar_t *function,
    const wchar_t *file, unsigned int line, uintptr_t reserved) {
    (void)expression; (void)function; (void)file; (void)line; (void)reserved;
}

static intptr_t atslib_fd_lookup(int fd) {
    if (fd < 0) { errno = EBADF; return -1; }
    _invalid_parameter_handler old =
        _set_thread_local_invalid_parameter_handler(atslib_fd_invalid_parameter);
    errno = 0;
    intptr_t raw = _get_osfhandle(fd);
    int saved = errno;
    _set_thread_local_invalid_parameter_handler(old);
    errno = saved;
    return raw;
}

static int atslib_win_fd_alive(int fd) {
    intptr_t raw = atslib_fd_lookup(fd);
    if (raw == (intptr_t)-1 || raw == (intptr_t)-2) {
        errno = EBADF;
        return 0;
    }
    DWORD flags;
    if (!GetHandleInformation((HANDLE)raw, &flags)) {
        errno = EBADF;
        return 0;
    }
    return 1;
}

int atslib_libats_win_fildes_iget_int(int fd) {
    return atslib_win_fd_alive(fd) ? fd : -1;
}

int atslib_libats_win_dup2_fildes(int src, int dst) {
    if (dst < 0) { errno = EBADF; return -1; }
    if (!atslib_win_fd_alive(src)) return -1;
    intptr_t raw = atslib_fd_lookup(dst);
    if (raw != (intptr_t)-1) { errno = EINVAL; return -1; }
    if (errno != EBADF) return -1;
    _invalid_parameter_handler old =
        _set_thread_local_invalid_parameter_handler(atslib_fd_invalid_parameter);
    int rc = _dup2(src, dst);
    int saved = errno;
    _set_thread_local_invalid_parameter_handler(old);
    errno = saved;
    return rc == 0 ? dst : -1;
}
#endif
