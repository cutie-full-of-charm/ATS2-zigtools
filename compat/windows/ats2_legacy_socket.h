#ifndef ATS2_LEGACY_SOCKET_H
#define ATS2_LEGACY_SOCKET_H

#include <stddef.h>
#include <stdint.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN 1
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
typedef unsigned short sa_family_t;
typedef unsigned short sp_family_t;
typedef uint16_t in_port_t;
typedef uint32_t in_addr_t;
typedef int ats2_legacy_socklen_t;
#else
#include <sys/socket.h>
#include <netinet/in.h>
typedef socklen_t ats2_legacy_socklen_t;
#endif

#ifdef __cplusplus
extern "C" {
#endif
int ats2_legacy_socket_open(int family, int type, int protocol);
int ats2_legacy_socket_bind(int virtual_fd, const struct sockaddr *address, ats2_legacy_socklen_t length);
int ats2_legacy_socket_listen(int virtual_fd, int backlog);
int ats2_legacy_socket_connect(int virtual_fd, const struct sockaddr *address, ats2_legacy_socklen_t length);
int ats2_legacy_socket_accept(int virtual_fd, struct sockaddr *address, ats2_legacy_socklen_t *length);
int ats2_legacy_socket_local(int virtual_fd, struct sockaddr *address, ats2_legacy_socklen_t *length);
int ats2_legacy_socket_close(int virtual_fd);
int ats2_legacy_socket_shutdown(int virtual_fd, int how);
ptrdiff_t ats2_legacy_socket_read(int virtual_fd, void *bytes, size_t length);
ptrdiff_t ats2_legacy_socket_write(int virtual_fd, const void *bytes, size_t length);
#ifdef __cplusplus
}
#endif

#endif
