#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "ats2_net_socket_internal.h"

static int fail(const char *name, int error) {
    fprintf(stderr, "socket contracts failed: %s (error=%d)\n", name, error);
    return 1;
}

#define CHECK(name, condition) do { if (!(condition)) return fail(name, error); } while (0)

int main(void) {
    int error = 0;
    ats2_net_socket sock;
    ats2_net_addr4 local = {{127, 0, 0, 1}, 0};
    ats2_net_addr4 endpoint;
    const char payload[] = {'A', '\0', (char)0xff, 'Z'};
    char receive[8] = {0};

    ats2_net_socket_init(NULL);
    CHECK("null-open", ats2_net_udp4_open(NULL, &error) == -1 && error == ATS2_NET_EINVAL);
    CHECK("null-close", ats2_net_close(NULL, &error) == -1 && error == ATS2_NET_ESTATE);
    CHECK("null-ready", !ats2_net_socket_is_open(NULL));

    ats2_net_socket_init(&sock);
    CHECK("init closed", !ats2_net_socket_is_open(&sock));
    CHECK("close before open", ats2_net_close(&sock, &error) == -1 && error == ATS2_NET_ESTATE);
    CHECK("bind before open", ats2_net_bind4(&sock, &local, &error) == -1 && error == ATS2_NET_EINVAL);
    CHECK("udp-open", ats2_net_udp4_open(&sock, &error) == 0 && error == 0);
    const uintptr_t handle = sock.handle;
    CHECK("double open", ats2_net_tcp4_open(&sock, &error) == -1 && error == ATS2_NET_ESTATE);
    CHECK("double open preserves", ats2_net_socket_is_open(&sock) && sock.handle == handle);
    CHECK("null bind", ats2_net_bind4(&sock, NULL, &error) == -1 && error == ATS2_NET_EINVAL);
    CHECK("null getsockname", ats2_net_local4(&sock, NULL, &error) == -1 && error == ATS2_NET_EINVAL);
    CHECK("null sendto address", ats2_net_sendto4(&sock, payload, sizeof(payload), NULL, &error) == -1 && error == ATS2_NET_EINVAL);
    CHECK("null sendto data", ats2_net_sendto4(&sock, NULL, 1, &local, &error) == -1 && error == ATS2_NET_EINVAL);
    CHECK("null recvfrom data", ats2_net_recvfrom4(&sock, NULL, 1, &endpoint, &error) == -1 && error == ATS2_NET_EINVAL);
    CHECK("null send", ats2_net_send(&sock, NULL, 1, &error) == -1 && error == ATS2_NET_EINVAL);
    CHECK("null recv", ats2_net_recv(&sock, NULL, 1, &error) == -1 && error == ATS2_NET_EINVAL);
    CHECK("zero backlog", ats2_net_listen(&sock, 0, &error) == -1 && error == ATS2_NET_EINVAL);
    CHECK("bad shutdown", ats2_net_shutdown(&sock, 3, &error) == -1 && error == ATS2_NET_EINVAL);
    CHECK("bad shutdown negative", ats2_net_shutdown(&sock, -1, &error) == -1 && error == ATS2_NET_EINVAL);
    if (sizeof(size_t) > sizeof(int)) {
        CHECK("huge send", ats2_net_send(&sock, payload, (size_t)INT_MAX + 1u, &error) == -1 && error == ATS2_NET_ERANGE);
        CHECK("huge recv", ats2_net_recv(&sock, receive, (size_t)INT_MAX + 1u, &error) == -1 && error == ATS2_NET_ERANGE);
    }
    CHECK("state preserved", ats2_net_socket_is_open(&sock) && sock.handle == handle);
    CHECK("bind after failures", ats2_net_bind4(&sock, &local, &error) == 0 && error == 0);
    CHECK("local after bind", ats2_net_local4(&sock, &endpoint, &error) == 0 && endpoint.port != 0);
    CHECK("loopback preserved", memcmp(endpoint.ip, local.ip, sizeof(local.ip)) == 0);
    CHECK("socket close", ats2_net_close(&sock, &error) == 0 && error == 0);
    CHECK("closed after close", !ats2_net_socket_is_open(&sock));
    CHECK("double close", ats2_net_close(&sock, &error) == -1 && error == ATS2_NET_ESTATE);

    for (int i = 0; i < 64; ++i) {
        ats2_net_socket_init(&sock);
        CHECK("repeat open", ats2_net_tcp4_open(&sock, &error) == 0);
        CHECK("repeat close", ats2_net_close(&sock, &error) == 0);
    }

    ats2_net_socket *owned = ats2_net_owned_udp4_open();
    CHECK("owned open", owned != NULL && ats2_net_socket_is_open(owned));
    CHECK("owned bind", ats2_net_owned_udp_bind4_state(owned, &local, &error) == 1);
    CHECK("owned try close", ats2_net_owned_try_close(owned, 1, &error) == -1 && error == 0);
    CHECK("owned consumed", !ats2_net_socket_is_open(owned));
    ats2_net_owned_free_closed(owned);

    puts("socket contracts ok");
    return 0;
}
