#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "ats2_net_socket_internal.h"

static int fail(const char *op, int err) {
    fprintf(stderr, "socket portable failed: %s (error=%d)\n", op, err);
    return 1;
}

#define CHECK(what, expr) do { if (!(expr)) return fail(what, err); } while (0)

int main(void) {
    int err = 0;
    ats2_net_socket udp_server, udp_client, tcp_server, tcp_client, tcp_accepted;
    ats2_net_socket_init(&udp_server);
    ats2_net_socket_init(&udp_client);
    ats2_net_socket_init(&tcp_server);
    ats2_net_socket_init(&tcp_client);
    ats2_net_socket_init(&tcp_accepted);

    ats2_net_addr4 loopback = {{127, 0, 0, 1}, 0};
    ats2_net_addr4 endpoint = {{0, 0, 0, 0}, 0};
    ats2_net_addr4 peer = {{0, 0, 0, 0}, 0};
    unsigned char buffer[16] = {0};
    const unsigned char datagram[] = {0, 1, 2, 0xff, 4, 5};

    CHECK("open UDP server", ats2_net_udp4_open(&udp_server, &err) == 0);
    CHECK("open UDP client", ats2_net_udp4_open(&udp_client, &err) == 0);
    CHECK("reject double open", ats2_net_udp4_open(&udp_server, &err) == -1 && err == ATS2_NET_ESTATE);
    CHECK("bind UDP", ats2_net_bind4(&udp_server, &loopback, &err) == 0);
    CHECK("UDP endpoint", ats2_net_local4(&udp_server, &endpoint, &err) == 0 && endpoint.port != 0);
    CHECK("UDP recv timeout", ats2_net_set_recv_timeout_ms(&udp_server, 2000, &err) == 0);
    CHECK("sendto UDP", ats2_net_sendto4(&udp_client, datagram, sizeof(datagram), &endpoint, &err) == (ptrdiff_t)sizeof(datagram));
    CHECK("recvfrom UDP", ats2_net_recvfrom4(&udp_server, buffer, sizeof(buffer), &peer, &err) == (ptrdiff_t)sizeof(datagram));
    CHECK("UDP payload", memcmp(datagram, buffer, sizeof(datagram)) == 0);
    CHECK("UDP peer", memcmp(peer.ip, loopback.ip, 4) == 0 && peer.port != 0);
    if (sizeof(size_t) > sizeof(int)) {
        CHECK("reject oversized UDP", ats2_net_sendto4(&udp_client, buffer, (size_t)INT_MAX + 1u, &endpoint, &err) == -1 && err == ATS2_NET_ERANGE);
    }
    CHECK("reject null UDP buffer", ats2_net_sendto4(&udp_client, NULL, 1, &endpoint, &err) == -1 && err == ATS2_NET_EINVAL);

    CHECK("open TCP server", ats2_net_tcp4_open(&tcp_server, &err) == 0);
    CHECK("bind TCP", ats2_net_bind4(&tcp_server, &loopback, &err) == 0);
    CHECK("listen TCP", ats2_net_listen(&tcp_server, 1, &err) == 0);
    CHECK("TCP endpoint", ats2_net_local4(&tcp_server, &endpoint, &err) == 0 && endpoint.port != 0);
    CHECK("open TCP client", ats2_net_tcp4_open(&tcp_client, &err) == 0);
    CHECK("connect TCP", ats2_net_connect4(&tcp_client, &endpoint, &err) == 0);
    CHECK("accept TCP", ats2_net_accept(&tcp_server, &tcp_accepted, &err) == 0);
    CHECK("TCP recv timeout", ats2_net_set_recv_timeout_ms(&tcp_accepted, 2000, &err) == 0);
    size_t sent = 0;
    while (sent < sizeof(datagram)) {
        ptrdiff_t n = ats2_net_send(&tcp_client, datagram + sent, sizeof(datagram) - sent, &err);
        CHECK("send TCP", n > 0);
        sent += (size_t)n;
    }
    size_t received = 0;
    while (received < sizeof(datagram)) {
        ptrdiff_t n = ats2_net_recv(&tcp_accepted, buffer + received, sizeof(datagram) - received, &err);
        CHECK("recv TCP", n > 0);
        received += (size_t)n;
    }
    CHECK("TCP payload", memcmp(datagram, buffer, sizeof(datagram)) == 0);
    CHECK("shutdown TCP send", ats2_net_shutdown(&tcp_client, 1, &err) == 0);
    CHECK("TCP EOF", ats2_net_recv(&tcp_accepted, buffer, 1, &err) == 0);
    CHECK("close accepted", ats2_net_close(&tcp_accepted, &err) == 0);
    CHECK("close client", ats2_net_close(&tcp_client, &err) == 0);
    CHECK("close server", ats2_net_close(&tcp_server, &err) == 0);
    CHECK("close UDP client", ats2_net_close(&udp_client, &err) == 0);
    CHECK("close UDP server", ats2_net_close(&udp_server, &err) == 0);
    CHECK("closed invalidated", !ats2_net_socket_is_open(&udp_server));
    CHECK("double close rejected", ats2_net_close(&udp_server, &err) == -1 && err == ATS2_NET_ESTATE);

    puts("socket portable ok");
    return 0;
}
