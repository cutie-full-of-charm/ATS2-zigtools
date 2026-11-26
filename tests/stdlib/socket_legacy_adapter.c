/* Exercise the legacy ATS2 int-descriptor compatibility adapter.
 * This is NOT a replacement for testing the ATS socket_v type interface. */
#include "ats2_legacy_socket.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "legacy adapter: %s:%d: %s (errno=%d)\n", __FILE__, __LINE__, #x, errno); return 1; } } while (0)
int main(void) {
    int server = ats2_legacy_socket_open(AF_INET, SOCK_STREAM, 0);
    CHECK(server > 0);
    CHECK(ats2_legacy_socket_close(-1) == -1);
    CHECK(ats2_legacy_socket_close(server) == 0);
    CHECK(ats2_legacy_socket_close(server) == -1);
    int listener = ats2_legacy_socket_open(AF_INET, SOCK_STREAM, 0);
    CHECK(listener > 0 && listener != server); /* old IDs must be rejected */
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(0);
    addr.sin_addr.s_addr = htonl(0x7f000001u);
    CHECK(ats2_legacy_socket_bind(listener, (struct sockaddr *)&addr, (ats2_legacy_socklen_t)sizeof(addr)) == 0);
    CHECK(ats2_legacy_socket_listen(listener, 1) == 0);
    ats2_legacy_socklen_t len = (ats2_legacy_socklen_t)sizeof(addr);
    CHECK(ats2_legacy_socket_local(listener, (struct sockaddr *)&addr, &len) == 0);
    CHECK(addr.sin_port != 0);
    int client = ats2_legacy_socket_open(AF_INET, SOCK_STREAM, 0);
    CHECK(client > 0);
    CHECK(ats2_legacy_socket_connect(client, (struct sockaddr *)&addr, (ats2_legacy_socklen_t)sizeof(addr)) == 0);
    int accepted = ats2_legacy_socket_accept(listener, NULL, NULL);
    CHECK(accepted > 0 && accepted != client);
    const char payload[] = "legacy socket";
    char response[sizeof(payload)] = {0};
    CHECK(ats2_legacy_socket_write(client, payload, sizeof(payload)) == (ptrdiff_t)sizeof(payload));
    CHECK(ats2_legacy_socket_read(accepted, response, sizeof(response)) == (ptrdiff_t)sizeof(payload));
    CHECK(memcmp(response, payload, sizeof(payload)) == 0);
    CHECK(ats2_legacy_socket_shutdown(client, 1) == 0);
    CHECK(ats2_legacy_socket_read(accepted, response, sizeof(response)) == 0);
    CHECK(ats2_legacy_socket_close(client) == 0);
    CHECK(ats2_legacy_socket_close(accepted) == 0);
    CHECK(ats2_legacy_socket_close(listener) == 0);
    CHECK(ats2_legacy_socket_close(listener) == -1);
    CHECK(ats2_legacy_socket_shutdown(listener, 1) == -1);
    puts("socket legacy adapter ok");
    return 0;
}
