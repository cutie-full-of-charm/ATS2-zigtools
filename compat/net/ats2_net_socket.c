#include "ats2_net_socket_internal.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
_Static_assert(sizeof(SOCKET) == sizeof(uintptr_t), "SOCKET must fit uintptr_t");
typedef SOCKET ats2_native_socket;
typedef int ats2_socklen;
#define ATS2_NATIVE_INVALID INVALID_SOCKET
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
typedef int ats2_native_socket;
typedef socklen_t ats2_socklen;
#define ATS2_NATIVE_INVALID (-1)
#endif

#define ATS2_NET_MAGIC UINT32_C(0xA752B2C5)

static int ats2_fail(int *error, int reason) {
    if (error) *error = reason;
    return -1;
}

static ptrdiff_t ats2_fail_size(int *error, int reason) {
    if (error) *error = reason;
    return -1;
}

static int ats2_native_error(void) {
#ifdef _WIN32
    return WSAGetLastError();
#else
    return errno;
#endif
}

static int ats2_ready(const ats2_net_socket *sock) {
    return sock && sock->magic == ATS2_NET_MAGIC && sock->handle != UINTPTR_MAX;
}

static ats2_native_socket ats2_native(const ats2_net_socket *sock) {
    return (ats2_native_socket)sock->handle;
}

static void ats2_encode_addr(struct sockaddr_in *sa, const ats2_net_addr4 *addr) {
    memset(sa, 0, sizeof(*sa));
    sa->sin_family = AF_INET;
    sa->sin_port = htons(addr->port);
    memcpy(&sa->sin_addr.s_addr, addr->ip, sizeof(addr->ip));
}

static void ats2_decode_addr(ats2_net_addr4 *addr, const struct sockaddr_in *sa) {
    memcpy(addr->ip, &sa->sin_addr.s_addr, sizeof(addr->ip));
    addr->port = ntohs(sa->sin_port);
}

static int ats2_startup(int *error) {
#ifdef _WIN32
    WSADATA wsa_data;
    int rc = WSAStartup(MAKEWORD(2, 2), &wsa_data);
    if (rc != 0) return ats2_fail(error, rc);
    if (LOBYTE(wsa_data.wVersion) != 2 || HIBYTE(wsa_data.wVersion) != 2) {
        WSACleanup();
        return ats2_fail(error, WSAVERNOTSUPPORTED);
    }
#else
    (void)error;
#endif
    return 0;
}

static void ats2_cancel_startup(void) {
#ifdef _WIN32
    WSACleanup();
#endif
}

void ats2_net_socket_init(ats2_net_socket *sock) {
    if (!sock) return;
    sock->handle = UINTPTR_MAX;
    sock->magic = ATS2_NET_MAGIC;
}

int ats2_net_socket_is_open(const ats2_net_socket *sock) {
    return ats2_ready(sock);
}

static int ats2_open(ats2_net_socket *out, int type, int *error) {
    if (!out || out->magic != ATS2_NET_MAGIC) return ats2_fail(error, ATS2_NET_EINVAL);
    if (out->handle != UINTPTR_MAX) return ats2_fail(error, ATS2_NET_ESTATE);
    if (ats2_startup(error) != 0) return -1;
    ats2_native_socket native = socket(AF_INET, type, 0);
    if (native == ATS2_NATIVE_INVALID) {
        int reason = ats2_native_error();
        ats2_cancel_startup();
        return ats2_fail(error, reason);
    }
    out->handle = (uintptr_t)native;
    if (error) *error = 0;
    return 0;
}

int ats2_net_udp4_open(ats2_net_socket *out, int *error) {
    return ats2_open(out, SOCK_DGRAM, error);
}

int ats2_net_tcp4_open(ats2_net_socket *out, int *error) {
    return ats2_open(out, SOCK_STREAM, error);
}

int ats2_net_bind4(ats2_net_socket *sock, const ats2_net_addr4 *addr, int *error) {
    if (!ats2_ready(sock) || !addr) return ats2_fail(error, ATS2_NET_EINVAL);
    struct sockaddr_in sa;
    ats2_encode_addr(&sa, addr);
    if (bind(ats2_native(sock), (const struct sockaddr *)&sa, (ats2_socklen)sizeof(sa)) != 0)
        return ats2_fail(error, ats2_native_error());
    if (error) *error = 0;
    return 0;
}

int ats2_net_local4(const ats2_net_socket *sock, ats2_net_addr4 *addr, int *error) {
    if (!ats2_ready(sock) || !addr) return ats2_fail(error, ATS2_NET_EINVAL);
    struct sockaddr_in sa;
    ats2_socklen sa_len = (ats2_socklen)sizeof(sa);
    memset(&sa, 0, sizeof(sa));
    if (getsockname(ats2_native(sock), (struct sockaddr *)&sa, &sa_len) != 0)
        return ats2_fail(error, ats2_native_error());
    if (sa_len < (ats2_socklen)sizeof(sa) || sa.sin_family != AF_INET)
        return ats2_fail(error, ATS2_NET_EINVAL);
    ats2_decode_addr(addr, &sa);
    if (error) *error = 0;
    return 0;
}

int ats2_net_listen(ats2_net_socket *sock, int backlog, int *error) {
    if (!ats2_ready(sock) || backlog <= 0) return ats2_fail(error, ATS2_NET_EINVAL);
    if (listen(ats2_native(sock), backlog) != 0)
        return ats2_fail(error, ats2_native_error());
    if (error) *error = 0;
    return 0;
}

int ats2_net_accept(ats2_net_socket *listener, ats2_net_socket *out, int *error) {
    if (!ats2_ready(listener) || !out || out->magic != ATS2_NET_MAGIC)
        return ats2_fail(error, ATS2_NET_EINVAL);
    if (out->handle != UINTPTR_MAX) return ats2_fail(error, ATS2_NET_ESTATE);
    if (ats2_startup(error) != 0) return -1;
    ats2_native_socket native = accept(ats2_native(listener), NULL, NULL);
    if (native == ATS2_NATIVE_INVALID) {
        int reason = ats2_native_error();
        ats2_cancel_startup();
        return ats2_fail(error, reason);
    }
    out->handle = (uintptr_t)native;
    if (error) *error = 0;
    return 0;
}

int ats2_net_connect4(ats2_net_socket *sock, const ats2_net_addr4 *addr, int *error) {
    if (!ats2_ready(sock) || !addr) return ats2_fail(error, ATS2_NET_EINVAL);
    struct sockaddr_in sa;
    ats2_encode_addr(&sa, addr);
    if (connect(ats2_native(sock), (const struct sockaddr *)&sa, (ats2_socklen)sizeof(sa)) != 0)
        return ats2_fail(error, ats2_native_error());
    if (error) *error = 0;
    return 0;
}

static int ats2_check_data(const ats2_net_socket *sock, const void *data, size_t length, int *error) {
    if (!ats2_ready(sock) || (length && !data)) return ats2_fail(error, ATS2_NET_EINVAL);
    if (length > INT_MAX) return ats2_fail(error, ATS2_NET_ERANGE);
    return 0;
}

static int ats2_send_flags(void) {
#if defined(MSG_NOSIGNAL) && !defined(_WIN32)
    return MSG_NOSIGNAL;
#else
    return 0;
#endif
}

ptrdiff_t ats2_net_send(ats2_net_socket *sock, const void *data, size_t length, int *error) {
    if (ats2_check_data(sock, data, length, error) != 0) return -1;
    const char *bytes = data ? (const char *)data : "";
#ifdef _WIN32
    int rc = send(ats2_native(sock), bytes, (int)length, ats2_send_flags());
#else
    ssize_t rc = send(ats2_native(sock), bytes, length, ats2_send_flags());
#endif
    if (rc < 0) return ats2_fail_size(error, ats2_native_error());
    if ((size_t)rc > length) return ats2_fail_size(error, ATS2_NET_ERANGE);
    if (error) *error = 0;
    return (ptrdiff_t)rc;
}

ptrdiff_t ats2_net_recv(ats2_net_socket *sock, void *data, size_t length, int *error) {
    if (ats2_check_data(sock, data, length, error) != 0) return -1;
    char scratch = 0;
    char *bytes = data ? (char *)data : &scratch;
#ifdef _WIN32
    int rc = recv(ats2_native(sock), bytes, (int)length, 0);
#else
    ssize_t rc = recv(ats2_native(sock), bytes, length, 0);
#endif
    if (rc < 0) return ats2_fail_size(error, ats2_native_error());
    if ((size_t)rc > length) return ats2_fail_size(error, ATS2_NET_ERANGE);
    if (error) *error = 0;
    return (ptrdiff_t)rc;
}

ptrdiff_t ats2_net_sendto4(ats2_net_socket *sock, const void *data, size_t length, const ats2_net_addr4 *addr, int *error) {
    if (ats2_check_data(sock, data, length, error) != 0) return -1;
    if (!addr) return ats2_fail_size(error, ATS2_NET_EINVAL);
    struct sockaddr_in sa;
    ats2_encode_addr(&sa, addr);
    const char *bytes = data ? (const char *)data : "";
#ifdef _WIN32
    int rc = sendto(ats2_native(sock), bytes, (int)length, 0, (const struct sockaddr *)&sa, (int)sizeof(sa));
#else
    ssize_t rc = sendto(ats2_native(sock), bytes, length, ats2_send_flags(), (const struct sockaddr *)&sa, (socklen_t)sizeof(sa));
#endif
    if (rc < 0) return ats2_fail_size(error, ats2_native_error());
    if ((size_t)rc > length) return ats2_fail_size(error, ATS2_NET_ERANGE);
    if (error) *error = 0;
    return (ptrdiff_t)rc;
}

ptrdiff_t ats2_net_recvfrom4(ats2_net_socket *sock, void *data, size_t length, ats2_net_addr4 *addr, int *error) {
    if (ats2_check_data(sock, data, length, error) != 0) return -1;
    struct sockaddr_in sa;
    ats2_socklen sa_len = (ats2_socklen)sizeof(sa);
    memset(&sa, 0, sizeof(sa));
    char scratch = 0;
    char *bytes = data ? (char *)data : &scratch;
#ifdef _WIN32
    int rc = recvfrom(ats2_native(sock), bytes, (int)length, 0, (struct sockaddr *)&sa, &sa_len);
#else
    ssize_t rc = recvfrom(ats2_native(sock), bytes, length, 0, (struct sockaddr *)&sa, &sa_len);
#endif
    if (rc < 0) return ats2_fail_size(error, ats2_native_error());
    if ((size_t)rc > length) return ats2_fail_size(error, ATS2_NET_ERANGE);
    if (sa_len < (ats2_socklen)sizeof(sa) || sa.sin_family != AF_INET)
        return ats2_fail_size(error, ATS2_NET_EINVAL);
    if (addr) ats2_decode_addr(addr, &sa);
    if (error) *error = 0;
    return (ptrdiff_t)rc;
}

int ats2_net_set_recv_timeout_ms(ats2_net_socket *sock, uint32_t ms, int *error) {
    if (!ats2_ready(sock)) return ats2_fail(error, ATS2_NET_EINVAL);
#ifdef _WIN32
    if (ms > (uint32_t)INT_MAX) return ats2_fail(error, ATS2_NET_ERANGE);
    DWORD timeout = (DWORD)ms;
    if (setsockopt(ats2_native(sock), SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, (int)sizeof(timeout)) != 0)
        return ats2_fail(error, ats2_native_error());
#else
    struct timeval timeout;
    timeout.tv_sec = (long)(ms / 1000);
    timeout.tv_usec = (long)(ms % 1000) * 1000;
    if (setsockopt(ats2_native(sock), SOL_SOCKET, SO_RCVTIMEO, (const void *)&timeout, (socklen_t)sizeof(timeout)) != 0)
        return ats2_fail(error, ats2_native_error());
#endif
    if (error) *error = 0;
    return 0;
}

int ats2_net_shutdown(ats2_net_socket *sock, int how, int *error) {
    if (!ats2_ready(sock) || how < 0 || how > 2) return ats2_fail(error, ATS2_NET_EINVAL);
#ifdef _WIN32
    const int mapped = how == 0 ? SD_RECEIVE : (how == 1 ? SD_SEND : SD_BOTH);
#else
    const int mapped = how;
#endif
    if (shutdown(ats2_native(sock), mapped) != 0)
        return ats2_fail(error, ats2_native_error());
    if (error) *error = 0;
    return 0;
}

int ats2_net_close(ats2_net_socket *sock, int *error) {
    if (!ats2_ready(sock)) return ats2_fail(error, ATS2_NET_ESTATE);
#ifdef _WIN32
    if (closesocket(ats2_native(sock)) != 0)
        return ats2_fail(error, ats2_native_error());
    sock->handle = UINTPTR_MAX;
    WSACleanup();
#else
    int rc = close(ats2_native(sock));
    int reason = rc == 0 ? 0 : ats2_native_error();
    sock->handle = UINTPTR_MAX;
    if (rc != 0) return ats2_fail(error, reason);
#endif
    if (error) *error = 0;
    return 0;
}

static ats2_net_socket *ats2_owned_open(int udp) {
    ats2_net_socket *sock = (ats2_net_socket *)malloc(sizeof(*sock));
    if (!sock) return NULL;
    ats2_net_socket_init(sock);
    int reason = 0;
    int rc = udp ? ats2_net_udp4_open(sock, &reason) : ats2_net_tcp4_open(sock, &reason);
    if (rc != 0) {
        free(sock);
        return NULL;
    }
    return sock;
}

ats2_net_socket *ats2_net_owned_udp4_open(void) {
    return ats2_owned_open(1);
}

ats2_net_socket *ats2_net_owned_tcp4_open(void) {
    return ats2_owned_open(0);
}

int ats2_net_owned_isnot_null(const ats2_net_socket *sock) {
    return sock != NULL;
}

void ats2_net_owned_close_exn(ats2_net_socket *sock) {
    int reason = 0;
    if (!sock || ats2_net_close(sock, &reason) != 0) abort();
    free(sock);
}

ats2_net_addr4 ats2_net_addr4_make(int a, int b, int c, int d, int port) {
    if ((unsigned)a > 255u || (unsigned)b > 255u ||
        (unsigned)c > 255u || (unsigned)d > 255u ||
        (unsigned)port > 65535u) abort();
    ats2_net_addr4 addr = {{(uint8_t)a, (uint8_t)b, (uint8_t)c, (uint8_t)d}, (uint16_t)port};
    return addr;
}

ats2_net_addr4 ats2_net_addr4_loopback(int port) {
    return ats2_net_addr4_make(127, 0, 0, 1, port);
}

int ats2_net_addr4_get_port(const ats2_net_addr4 *addr) {
    if (!addr) abort();
    return (int)addr->port;
}

int ats2_net_owned_udp_bind4_state(ats2_net_socket *sock, const ats2_net_addr4 *addr, int *error) {
    return ats2_net_bind4(sock, addr, error) == 0 ? 1 : 0;
}

int ats2_net_owned_tcp_bind4_state(ats2_net_socket *sock, const ats2_net_addr4 *addr, int *error) {
    return ats2_net_bind4(sock, addr, error) == 0 ? 1 : 0;
}

int ats2_net_owned_tcp_listen_state(ats2_net_socket *sock, int backlog, int *error) {
    return ats2_net_listen(sock, backlog, error) == 0 ? 2 : 1;
}

int ats2_net_owned_tcp_connect4_state(ats2_net_socket *sock, const ats2_net_addr4 *addr, int *error) {
    return ats2_net_connect4(sock, addr, error) == 0 ? 3 : 0;
}

ats2_net_socket *ats2_net_owned_tcp_accept(ats2_net_socket *listener, int *error) {
    ats2_net_socket *out = (ats2_net_socket *)malloc(sizeof(*out));
    if (!out) {
        ats2_fail(error, ATS2_NET_ENOMEM);
        return NULL;
    }
    ats2_net_socket_init(out);
    if (ats2_net_accept(listener, out, error) != 0) {
        free(out);
        return NULL;
    }
    return out;
}

int ats2_net_owned_try_close(ats2_net_socket *sock, int state, int *error) {
    if (!sock || sock->magic != ATS2_NET_MAGIC) {
        ats2_fail(error, ATS2_NET_EINVAL);
        return state;
    }
    if (sock->handle == UINTPTR_MAX) {
        if (error) *error = 0;
        return -1;
    }
    if (ats2_net_close(sock, error) == 0) return -1;
#ifdef _WIN32
    return state;
#else
    return -1;
#endif
}

void ats2_net_owned_free_closed(ats2_net_socket *sock) {
    if (!sock || sock->magic != ATS2_NET_MAGIC || sock->handle != UINTPTR_MAX) abort();
    sock->magic = 0;
    free(sock);
}

int ats2_net_owned_local4(const ats2_net_socket *sock, ats2_net_addr4 *addr, int *error) {
    if (!addr) return ats2_fail(error, ATS2_NET_EINVAL);
    memset(addr, 0, sizeof(*addr));
    return ats2_net_local4(sock, addr, error);
}

ptrdiff_t ats2_net_owned_udp_recvfrom4(ats2_net_socket *sock, void *data, size_t length, ats2_net_addr4 *addr, int *error) {
    if (!addr) return ats2_fail_size(error, ATS2_NET_EINVAL);
    memset(addr, 0, sizeof(*addr));
    return ats2_net_recvfrom4(sock, data, length, addr, error);
}
