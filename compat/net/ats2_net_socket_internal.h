#ifndef ATS2_NET_SOCKET_INTERNAL_H
#define ATS2_NET_SOCKET_INTERNAL_H
#include "ats2_net_socket.h"
#ifdef __cplusplus
extern "C" {
#endif
struct ats2_net_socket {
    uintptr_t handle;
    uint32_t magic;
};
void ats2_net_socket_init(ats2_net_socket *sock);
int ats2_net_udp4_open(ats2_net_socket *out, int *error);
int ats2_net_tcp4_open(ats2_net_socket *out, int *error);
int ats2_net_accept(ats2_net_socket *listener, ats2_net_socket *out, int *error);
int ats2_net_close(ats2_net_socket *sock, int *error);
#ifdef __cplusplus
}
#endif
#endif
