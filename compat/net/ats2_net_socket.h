#ifndef ATS2_NET_SOCKET_H
#define ATS2_NET_SOCKET_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
    ATS2_NET_EINVAL = -1,
    ATS2_NET_ESTATE = -2,
    ATS2_NET_ERANGE = -3,
    ATS2_NET_ENOMEM = -4
};

typedef struct ats2_net_socket ats2_net_socket;

typedef struct ats2_net_addr4 {
    uint8_t ip[4];
    uint16_t port;
} ats2_net_addr4;

int ats2_net_socket_is_open(const ats2_net_socket *sock);
int ats2_net_bind4(ats2_net_socket *sock, const ats2_net_addr4 *addr, int *error);
int ats2_net_local4(const ats2_net_socket *sock, ats2_net_addr4 *addr, int *error);
int ats2_net_listen(ats2_net_socket *sock, int backlog, int *error);
int ats2_net_connect4(ats2_net_socket *sock, const ats2_net_addr4 *addr, int *error);
ptrdiff_t ats2_net_send(ats2_net_socket *sock, const void *data, size_t length, int *error);
ptrdiff_t ats2_net_recv(ats2_net_socket *sock, void *data, size_t length, int *error);
ptrdiff_t ats2_net_sendto4(ats2_net_socket *sock, const void *data, size_t length, const ats2_net_addr4 *addr, int *error);
ptrdiff_t ats2_net_recvfrom4(ats2_net_socket *sock, void *data, size_t length, ats2_net_addr4 *addr, int *error);
int ats2_net_set_recv_timeout_ms(ats2_net_socket *sock, uint32_t ms, int *error);
int ats2_net_shutdown(ats2_net_socket *sock, int how, int *error);
ats2_net_socket *ats2_net_owned_udp4_open(void);
ats2_net_socket *ats2_net_owned_tcp4_open(void);
int ats2_net_owned_isnot_null(const ats2_net_socket *sock);
void ats2_net_owned_close_exn(ats2_net_socket *sock);

ats2_net_addr4 ats2_net_addr4_make(int a, int b, int c, int d, int port);
ats2_net_addr4 ats2_net_addr4_loopback(int port);
int ats2_net_addr4_get_port(const ats2_net_addr4 *addr);
int ats2_net_owned_local4(const ats2_net_socket *sock, ats2_net_addr4 *addr, int *error);
ptrdiff_t ats2_net_owned_udp_recvfrom4(ats2_net_socket *sock, void *data, size_t length, ats2_net_addr4 *addr, int *error);
int ats2_net_owned_udp_bind4_state(ats2_net_socket *sock, const ats2_net_addr4 *addr, int *error);
int ats2_net_owned_tcp_bind4_state(ats2_net_socket *sock, const ats2_net_addr4 *addr, int *error);
int ats2_net_owned_tcp_listen_state(ats2_net_socket *sock, int backlog, int *error);
int ats2_net_owned_tcp_connect4_state(ats2_net_socket *sock, const ats2_net_addr4 *addr, int *error);
ats2_net_socket *ats2_net_owned_tcp_accept(ats2_net_socket *listener, int *error);
int ats2_net_owned_try_close(ats2_net_socket *sock, int state, int *error);
void ats2_net_owned_free_closed(ats2_net_socket *sock);

#ifdef __cplusplus
}
#endif

#endif
