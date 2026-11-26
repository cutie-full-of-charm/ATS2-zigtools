#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "ats2_net_socket.h"

static int fail(const char *where, int err) {
    fprintf(stderr, "socket typed portable failed: %s (%d)\n", where, err);
    return 1;
}

#define MUST(name, test) do { if (!(test)) return fail(name, error); } while (0)

static int closed(ats2_net_socket *sock, int state) {
    int error = 0;
    if (ats2_net_owned_try_close(sock, state, &error) != -1) return fail("try-close", error);
    if (ats2_net_socket_is_open(sock)) return fail("still open after close", error);
    ats2_net_owned_free_closed(sock);
    return 0;
}

int main(void) {
    int error = 0;
    ats2_net_addr4 loopback = ats2_net_addr4_loopback(0);
    ats2_net_addr4 endpoint;
    ats2_net_addr4 peer;
    const char tx[] = {'A', '\0', (char)0xff, 'B'};
    char rx[16] = {0};
    ats2_net_socket *udp = ats2_net_owned_udp4_open();
    MUST("UDP allocation", udp != NULL);
    MUST("UDP bind state", ats2_net_owned_udp_bind4_state(udp, &loopback, &error) == 1);
    MUST("UDP endpoint", ats2_net_owned_local4(udp, &endpoint, &error) == 0);
    MUST("UDP port", ats2_net_addr4_get_port(&endpoint) > 0);
    MUST("UDP timeout", ats2_net_set_recv_timeout_ms(udp, 2000, &error) == 0);
    MUST("UDP sendto", ats2_net_sendto4(udp, tx, sizeof(tx), &endpoint, &error) == (ptrdiff_t)sizeof(tx));
    MUST("UDP recvfrom", ats2_net_owned_udp_recvfrom4(udp, rx, sizeof(rx), &peer, &error) == (ptrdiff_t)sizeof(tx));
    MUST("UDP data", memcmp(tx, rx, sizeof(tx)) == 0);
    MUST("UDP peer", memcmp(peer.ip, loopback.ip, 4) == 0 && peer.port != 0);
    if (sizeof(size_t) > sizeof(int)) {
        MUST("oversized send", ats2_net_sendto4(udp, tx, (size_t)INT_MAX + 1u, &endpoint, &error) == -1 && error == ATS2_NET_ERANGE);
    }
    MUST("UDP free", closed(udp, 1) == 0);

    ats2_net_socket *server = ats2_net_owned_tcp4_open();
    MUST("server allocation", server != NULL);
    MUST("server bind", ats2_net_owned_tcp_bind4_state(server, &loopback, &error) == 1);
    MUST("server listen", ats2_net_owned_tcp_listen_state(server, 2, &error) == 2);
    MUST("server endpoint", ats2_net_owned_local4(server, &endpoint, &error) == 0);
    ats2_net_socket *client = ats2_net_owned_tcp4_open();
    MUST("client allocation", client != NULL);
    MUST("client connect", ats2_net_owned_tcp_connect4_state(client, &endpoint, &error) == 3);
    ats2_net_socket *accepted = ats2_net_owned_tcp_accept(server, &error);
    MUST("accept", accepted != NULL);
    MUST("accepted timeout", ats2_net_set_recv_timeout_ms(accepted, 2000, &error) == 0);
    size_t transmitted = 0;
    while (transmitted < sizeof(tx)) {
        ptrdiff_t n = ats2_net_send(client, tx + transmitted, sizeof(tx) - transmitted, &error);
        MUST("send", n > 0);
        transmitted += (size_t)n;
    }
    size_t count = 0;
    while (count < sizeof(tx)) {
        ptrdiff_t n = ats2_net_recv(accepted, rx + count, sizeof(tx) - count, &error);
        MUST("receive", n > 0);
        count += (size_t)n;
    }
    MUST("TCP data", memcmp(tx, rx, sizeof(tx)) == 0);
    MUST("accepted free", closed(accepted, 3) == 0);
    MUST("client free", closed(client, 3) == 0);
    MUST("server free", closed(server, 2) == 0);

    puts("socket typed portable ok");
    return 0;
}
