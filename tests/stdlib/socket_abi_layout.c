#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "ats2_net_socket_internal.h"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
_Static_assert(sizeof(SOCKET) == sizeof(uintptr_t), "SOCKET pointer-width ABI");
#endif

_Static_assert(sizeof(uintptr_t) == sizeof(void *), "native-handle width");
_Static_assert(sizeof(uint16_t) == 2 && sizeof(uint32_t) == 4, "integer ABI");
_Static_assert(offsetof(ats2_net_socket, handle) == 0, "handle offset");
_Static_assert(offsetof(ats2_net_socket, magic) == sizeof(uintptr_t), "magic offset");
_Static_assert(offsetof(ats2_net_addr4, ip) == 0, "IPv4 octets offset");
_Static_assert(offsetof(ats2_net_addr4, port) == 4, "port offset");
_Static_assert(sizeof(ats2_net_addr4) == 6, "IPv4 address ABI");

int main(void) {
    printf("socket ABI ok: pointer=%zu socket=%zu magic_offset=%zu addr4=%zu port_offset=%zu\n",
           sizeof(void *), sizeof(ats2_net_socket),
           offsetof(ats2_net_socket, magic), sizeof(ats2_net_addr4),
           offsetof(ats2_net_addr4, port));
    return 0;
}
