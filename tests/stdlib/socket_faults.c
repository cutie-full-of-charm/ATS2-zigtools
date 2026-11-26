#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET test_socket_handle;
typedef int test_socklen;
#define TEST_INVALID_SOCKET INVALID_SOCKET
#define TEST_FAILURE SOCKET_ERROR
#define TEST_ERROR WSAENOBUFS
#define TEST_FAIL_ERR() WSASetLastError(TEST_ERROR)
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
typedef int test_socket_handle;
typedef socklen_t test_socklen;
#define TEST_INVALID_SOCKET (-1)
#define TEST_FAILURE (-1)
#define TEST_ERROR EMFILE
#define TEST_FAIL_ERR() (errno = TEST_ERROR)
#endif

#include "ats2_net_socket_internal.h"

enum fault_site {
    FAIL_NONE,
    FAIL_ALLOC,
    FAIL_STARTUP,
    FAIL_SOCKET,
    FAIL_BIND,
    FAIL_ACCEPT,
    FAIL_SEND,
    FAIL_RECV,
    FAIL_CLOSE,
    BAD_SEND_COUNT,
    BAD_RECV_COUNT
};

static enum fault_site fault_site;
static int live_allocations;
static int live_sockets;
static int live_startups;
static int intercepted;

static void *test_malloc(size_t n) {
    if (fault_site == FAIL_ALLOC) { intercepted++; return NULL; }
    void *p = malloc(n);
    if (p) live_allocations++;
    return p;
}

static void test_free(void *p) {
    if (p) live_allocations--;
    free(p);
}

static test_socket_handle test_socket(int domain, int type, int protocol) {
    if (fault_site == FAIL_SOCKET) {
        intercepted++;
        TEST_FAIL_ERR();
        return TEST_INVALID_SOCKET;
    }
    test_socket_handle s = socket(domain, type, protocol);
    if (s != TEST_INVALID_SOCKET) live_sockets++;
    return s;
}

static int test_bind(test_socket_handle s, const struct sockaddr *addr, test_socklen n) {
    if (fault_site == FAIL_BIND) {
        intercepted++;
        TEST_FAIL_ERR();
        return TEST_FAILURE;
    }
    return bind(s, addr, n);
}

static test_socket_handle test_accept(test_socket_handle s, struct sockaddr *addr, test_socklen *n) {
    if (fault_site == FAIL_ACCEPT) {
        intercepted++;
        TEST_FAIL_ERR();
        return TEST_INVALID_SOCKET;
    }
    test_socket_handle accepted = accept(s, addr, n);
    if (accepted != TEST_INVALID_SOCKET) live_sockets++;
    return accepted;
}

#ifdef _WIN32
static int test_close_socket(test_socket_handle s) {
    if (fault_site == FAIL_CLOSE) {
        intercepted++;
        TEST_FAIL_ERR();
        return SOCKET_ERROR;
    }
    int rc = closesocket(s);
    if (rc == 0) live_sockets--;
    return rc;
}

static int test_wsa_startup(WORD version, LPWSADATA data) {
    if (fault_site == FAIL_STARTUP) {
        intercepted++;
        return WSASYSNOTREADY;
    }
    int rc = WSAStartup(version, data);
    if (rc == 0) live_startups++;
    return rc;
}

static int test_wsa_cleanup(void) {
    int rc = WSACleanup();
    if (rc == 0) live_startups--;
    return rc;
}

static int test_send(test_socket_handle s, const char *data, int length, int flags) {
    if (fault_site == BAD_SEND_COUNT) { intercepted++; return length + 1; }
    if (fault_site == FAIL_SEND) {
        intercepted++;
        TEST_FAIL_ERR();
        return SOCKET_ERROR;
    }
    return send(s, data, length, flags);
}

static int test_recv(test_socket_handle s, char *data, int length, int flags) {
    if (fault_site == BAD_RECV_COUNT) { intercepted++; return length + 1; }
    if (fault_site == FAIL_RECV) {
        intercepted++;
        TEST_FAIL_ERR();
        return SOCKET_ERROR;
    }
    return recv(s, data, length, flags);
}
#else
static int test_close_socket(test_socket_handle s) {
    int rc = close(s);
    if (rc == 0) live_sockets--;
    if (fault_site == FAIL_CLOSE && rc == 0) {
        intercepted++;
        errno = EINTR;
        return -1;
    }
    return rc;
}

static ssize_t test_send(test_socket_handle s, const void *data, size_t length, int flags) {
    if (fault_site == BAD_SEND_COUNT) { intercepted++; return length + 1; }
    if (fault_site == FAIL_SEND) {
        intercepted++;
        TEST_FAIL_ERR();
        return -1;
    }
    return send(s, data, length, flags);
}

static ssize_t test_recv(test_socket_handle s, void *data, size_t length, int flags) {
    if (fault_site == BAD_RECV_COUNT) { intercepted++; return length + 1; }
    if (fault_site == FAIL_RECV) {
        intercepted++;
        TEST_FAIL_ERR();
        return -1;
    }
    return recv(s, data, length, flags);
}
#endif

#define malloc test_malloc
#define free test_free
#define socket test_socket
#define bind test_bind
#define accept test_accept
#define send test_send
#define recv test_recv
#ifdef _WIN32
#define closesocket test_close_socket
#define WSAStartup test_wsa_startup
#define WSACleanup test_wsa_cleanup
#else
#define close test_close_socket
#endif
#include "../../compat/net/ats2_net_socket.c"
#undef malloc
#undef free
#undef socket
#undef bind
#undef accept
#undef send
#undef recv
#ifdef _WIN32
#undef closesocket
#undef WSAStartup
#undef WSACleanup
#else
#undef close
#endif

static int fail(const char *name, int error) {
    fprintf(stderr, "socket faults failed: %s (error=%d, allocs=%d, sockets=%d, startup=%d, intercepted=%d)\n",
            name, error, live_allocations, live_sockets, live_startups, intercepted);
    return 1;
}

#define CHECK(name, expression) do { if (!(expression)) return fail((name), error); } while (0)
#define CLEAN(name) CHECK((name), live_allocations == 0 && live_sockets == 0 && live_startups == 0)
#define INJECT(site) do { fault_site = (site); intercepted = 0; } while (0)
#define HIT(name) do { CHECK((name), intercepted == 1); INJECT(FAIL_NONE); } while (0)

static int close_owned(ats2_net_socket *sock, int state) {
    int error = 0;
    if (ats2_net_owned_try_close(sock, state, &error) != -1 || ats2_net_socket_is_open(sock))
        return fail("close_owned", error);
    ats2_net_owned_free_closed(sock);
    return 0;
}

int main(void) {
    int error = 0;
    ats2_net_addr4 loopback = {{127, 0, 0, 1}, 0};

    INJECT(FAIL_ALLOC);
    CHECK("malloc injection", ats2_net_owned_udp4_open() == NULL);
    HIT("malloc reached");
    CLEAN("malloc cleanup");

#ifdef _WIN32
    ats2_net_socket stack_sock;
    ats2_net_socket_init(&stack_sock);
    INJECT(FAIL_STARTUP);
    CHECK("startup injection", ats2_net_udp4_open(&stack_sock, &error) == -1 && error == WSASYSNOTREADY);
    HIT("startup reached");
    CHECK("startup state", !ats2_net_socket_is_open(&stack_sock));
    CLEAN("startup cleanup");
#endif

    ats2_net_socket raw;
    ats2_net_socket_init(&raw);
    INJECT(FAIL_SOCKET);
    CHECK("socket injection", ats2_net_udp4_open(&raw, &error) == -1 && error == TEST_ERROR);
    HIT("socket reached");
    CHECK("socket state", !ats2_net_socket_is_open(&raw));
    CLEAN("socket cleanup");

    ats2_net_socket *udp = ats2_net_owned_udp4_open();
    CHECK("open udp", udp != NULL);
    CHECK("baseline count", live_allocations == 1 && live_sockets == 1);
#ifdef _WIN32
    CHECK("baseline startup", live_startups == 1);
#endif
    const uintptr_t handle = udp->handle;
    INJECT(FAIL_BIND);
    CHECK("bind injection", ats2_net_owned_udp_bind4_state(udp, &loopback, &error) == 0 && error == TEST_ERROR);
    HIT("bind reached");
    CHECK("bind state", ats2_net_socket_is_open(udp) && udp->handle == handle);

    const char payload[] = "data";
    char recvbuf[8] = "xxxxxxx";
    INJECT(FAIL_SEND);
    CHECK("send injection", ats2_net_send(udp, payload, sizeof(payload), &error) == -1 && error == TEST_ERROR);
    HIT("send reached");
    CHECK("send state", udp->handle == handle);
    INJECT(FAIL_RECV);
    CHECK("recv injection", ats2_net_recv(udp, recvbuf, 7, &error) == -1 && error == TEST_ERROR);
    HIT("recv reached");
    CHECK("recv untouched", memcmp(recvbuf, "xxxxxxx", 8) == 0);

    INJECT(BAD_SEND_COUNT);
    CHECK("invalid send count", ats2_net_send(udp, payload, sizeof(payload), &error) == -1 && error == ATS2_NET_ERANGE);
    HIT("invalid send count reached");
    INJECT(BAD_RECV_COUNT);
    CHECK("invalid recv count", ats2_net_recv(udp, recvbuf, 7, &error) == -1 && error == ATS2_NET_ERANGE);
    HIT("invalid recv count reached");
    CHECK("invalid count state", ats2_net_socket_is_open(udp) && udp->handle == handle);

    INJECT(FAIL_CLOSE);
    int state = ats2_net_owned_try_close(udp, 0, &error);
    CHECK("close injection", intercepted == 1 && error != 0);
#ifdef _WIN32
    CHECK("windows handle retained", state == 0 && ats2_net_socket_is_open(udp) && udp->handle == handle);
    CHECK("windows startup retained", live_startups == 1 && live_sockets == 1);
    INJECT(FAIL_NONE);
    CHECK("retry close", close_owned(udp, 0) == 0);
#else
    CHECK("linux close consumed", state == -1 && !ats2_net_socket_is_open(udp));
    INJECT(FAIL_NONE);
    ats2_net_owned_free_closed(udp);
#endif
    CLEAN("close cleanup");

    ats2_net_socket *server = ats2_net_owned_tcp4_open();
    CHECK("server allocation", server != NULL);
    CHECK("server bind", ats2_net_owned_tcp_bind4_state(server, &loopback, &error) == 1);
    CHECK("server listen", ats2_net_owned_tcp_listen_state(server, 1, &error) == 2);
    CHECK("server count", live_allocations == 1 && live_sockets == 1);

    INJECT(FAIL_ALLOC);
    CHECK("accept malloc injection", ats2_net_owned_tcp_accept(server, &error) == NULL && error == ATS2_NET_ENOMEM);
    HIT("accept malloc reached");
    CHECK("accept malloc cleanup", live_allocations == 1 && live_sockets == 1);

    INJECT(FAIL_ACCEPT);
    CHECK("accept syscall injection", ats2_net_owned_tcp_accept(server, &error) == NULL && error == TEST_ERROR);
    HIT("accept syscall reached");
    CHECK("accept allocation cleanup", live_allocations == 1 && live_sockets == 1);
#ifdef _WIN32
    CHECK("accept startup cleanup", live_startups == 1);
#endif
    CHECK("listener preserved", ats2_net_socket_is_open(server));
    CHECK("server close", close_owned(server, 2) == 0);
    CLEAN("all cleanup");

    puts("socket faults ok");
    return 0;
}
