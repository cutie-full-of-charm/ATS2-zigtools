#include "ats2_legacy_socket.h"
#include <errno.h>
#include <limits.h>
#include <stdatomic.h>
#include <string.h>

#ifdef _WIN32
#define ATS2_NATIVE_BAD INVALID_SOCKET
typedef SOCKET ats2_native_socket;
#else
#include <unistd.h>
#define ATS2_NATIVE_BAD (-1)
typedef int ats2_native_socket;
#endif

#define ATS2_SOCK_SLOTS 2048u
#define ATS2_SOCK_SLOT_BITS 12u
#define ATS2_SOCK_SLOT_MASK ((1u << ATS2_SOCK_SLOT_BITS) - 1u)
#define ATS2_SOCK_MAX_GENERATION ((unsigned)INT_MAX >> ATS2_SOCK_SLOT_BITS)

typedef struct {
    ats2_native_socket native;
    unsigned generation;
    int used;
} ats2_socket_entry;
static ats2_socket_entry ats2_entries[ATS2_SOCK_SLOTS];
static atomic_flag ats2_mutex = ATOMIC_FLAG_INIT;
#ifdef _WIN32
static int ats2_wsa_state = 0;
#endif

static void ats2_lock(void) {
    while (atomic_flag_test_and_set_explicit(&ats2_mutex, memory_order_acquire)) { }
}
static void ats2_unlock(void) {
    atomic_flag_clear_explicit(&ats2_mutex, memory_order_release);
}
static int ats2_bad_descriptor(void) {
    errno = EBADF;
#ifdef _WIN32
    WSASetLastError(WSAENOTSOCK);
#endif
    return -1;
}

static void ats2_last_socket_error(void) {
#ifdef _WIN32
    int e = WSAGetLastError();
    switch (e) {
        case WSAEWOULDBLOCK: errno = EAGAIN; break;
        case WSAEINVAL: errno = EINVAL; break;
        case WSAEACCES: errno = EACCES; break;
        case WSAECONNRESET: errno = ECONNRESET; break;
        case WSAENOTSOCK: errno = EBADF; break;
        default: errno = EIO; break;
    }
#endif
}

static int ats2_decode(int virtual_fd, ats2_native_socket *native) {
    unsigned value = (unsigned)virtual_fd;
    unsigned slot = (value & ATS2_SOCK_SLOT_MASK);
    unsigned generation = value >> ATS2_SOCK_SLOT_BITS;
    if (virtual_fd <= 0 || slot == 0 || slot > ATS2_SOCK_SLOTS || generation == 0)
        return ats2_bad_descriptor();
    const ats2_socket_entry *entry = &ats2_entries[slot - 1];
    if (!entry->used || entry->generation != generation) return ats2_bad_descriptor();
    *native = entry->native;
    return 0;
}
static int ats2_register(ats2_native_socket native) {
    unsigned i;
    ats2_lock();
    for (i = 0; i < ATS2_SOCK_SLOTS; ++i) {
        ats2_socket_entry *entry = &ats2_entries[i];
        if (!entry->used && entry->generation < ATS2_SOCK_MAX_GENERATION) {
            ++entry->generation;
            entry->used = 1;
            entry->native = native;
            int id = (int)((entry->generation << ATS2_SOCK_SLOT_BITS) | (i + 1));
            ats2_unlock();
            return id;
        }
    }
    ats2_unlock();
    errno = EMFILE;
    return -1;
}

#ifdef _WIN32
static int ats2_ensure_wsa(void) {
    ats2_lock();
    if (ats2_wsa_state == 0) {
        WSADATA ws;
        int code = WSAStartup(MAKEWORD(2, 2), &ws);
        if (code != 0) {
            WSASetLastError(code);
            errno = EIO;
            ats2_unlock();
            return -1;
        }
        ats2_wsa_state = 1;
    }
    ats2_unlock();
    return 0;
}
static int ats2_close_native(ats2_native_socket native) { return closesocket(native); }
#else
static int ats2_ensure_wsa(void) { return 0; }
static int ats2_close_native(ats2_native_socket native) { return close(native); }
#endif

int ats2_legacy_socket_open(int family, int type, int protocol) {
    if (ats2_ensure_wsa() != 0) return -1;
    ats2_native_socket sock = socket(family, type, protocol);
    if (sock == ATS2_NATIVE_BAD) { ats2_last_socket_error(); return -1; }
    int id = ats2_register(sock);
    if (id < 0) ats2_close_native(sock);
    return id;
}
static int ats2_resolve(int virtual_fd, ats2_native_socket *native) {
    ats2_lock();
    int result = ats2_decode(virtual_fd, native);
    ats2_unlock();
    return result;
}
int ats2_legacy_socket_bind(int id, const struct sockaddr *sa, ats2_legacy_socklen_t len) {
    ats2_native_socket sock;
    if (ats2_resolve(id, &sock) < 0) return -1;
    int rc = bind(sock, sa, (int)len);
    if (rc < 0) ats2_last_socket_error();
    return rc;
}
int ats2_legacy_socket_listen(int id, int backlog) {
    ats2_native_socket sock;
    if (ats2_resolve(id, &sock) < 0) return -1;
    int rc = listen(sock, backlog);
    if (rc < 0) ats2_last_socket_error();
    return rc;
}
int ats2_legacy_socket_connect(int id, const struct sockaddr *sa, ats2_legacy_socklen_t len) {
    ats2_native_socket sock;
    if (ats2_resolve(id, &sock) < 0) return -1;
    int rc = connect(sock, sa, (int)len);
    if (rc < 0) ats2_last_socket_error();
    return rc;
}
int ats2_legacy_socket_accept(int id, struct sockaddr *sa, ats2_legacy_socklen_t *len) {
    ats2_native_socket sock;
    if (ats2_resolve(id, &sock) < 0) return -1;
    ats2_native_socket accepted = accept(sock, sa, len);
    if (accepted == ATS2_NATIVE_BAD) { ats2_last_socket_error(); return -1; }
    int new_id = ats2_register(accepted);
    if (new_id < 0) ats2_close_native(accepted);
    return new_id;
}
int ats2_legacy_socket_local(int id, struct sockaddr *sa, ats2_legacy_socklen_t *length) {
    ats2_native_socket sock;
    if (ats2_resolve(id, &sock) < 0) return -1;
    int rc = getsockname(sock, sa, length);
    if (rc < 0) ats2_last_socket_error();
    return rc;
}
int ats2_legacy_socket_close(int id) {
    ats2_native_socket sock;
    ats2_lock();
    if (ats2_decode(id, &sock) < 0) { ats2_unlock(); return -1; }
    unsigned slot = ((unsigned)id & ATS2_SOCK_SLOT_MASK) - 1;
    ats2_entries[slot].used = 0;
    ats2_unlock();
    int rc = ats2_close_native(sock);
    if (rc < 0) ats2_last_socket_error();
    return rc;
}
int ats2_legacy_socket_shutdown(int id, int how) {
    ats2_native_socket sock;
    if (ats2_resolve(id, &sock) < 0) return -1;
    int rc = shutdown(sock, how);
    if (rc < 0) ats2_last_socket_error();
    return rc;
}
ptrdiff_t ats2_legacy_socket_read(int id, void *bytes, size_t length) {
    ats2_native_socket sock;
    if (ats2_resolve(id, &sock) < 0) return -1;
    if ((!bytes && length) || length > INT_MAX) { errno = EINVAL; return -1; }
    char dummy = 0;
#ifdef _WIN32
    int rc = recv(sock, bytes ? (char *)bytes : &dummy, (int)length, 0);
#else
    ssize_t rc = recv(sock, bytes ? bytes : &dummy, length, 0);
#endif
    if (rc < 0) ats2_last_socket_error();
    return (ptrdiff_t)rc;
}
ptrdiff_t ats2_legacy_socket_write(int id, const void *bytes, size_t length) {
    ats2_native_socket sock;
    if (ats2_resolve(id, &sock) < 0) return -1;
    if ((!bytes && length) || length > INT_MAX) { errno = EINVAL; return -1; }
    char dummy = 0;
#ifdef _WIN32
    int rc = send(sock, bytes ? (const char *)bytes : &dummy, (int)length, 0);
#else
    ssize_t rc = send(sock, bytes ? bytes : &dummy, length, 0);
#endif
    if (rc < 0) ats2_last_socket_error();
    return (ptrdiff_t)rc;
}
