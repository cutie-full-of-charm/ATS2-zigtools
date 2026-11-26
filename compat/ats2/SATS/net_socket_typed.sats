%{#
#include "ats2_net_socket.h"
%}

absvtype netsock_vtype(l:addr, p:int, q:int) = ptr
vtypedef NetSock0(p:int, q:int) = [l:addr | l >= null] netsock_vtype(l, p, q)
vtypedef NetSock1(p:int, q:int) = [l:addr | l > null] netsock_vtype(l, p, q)

abst@ype netaddr4 = $extype"ats2_net_addr4"
fun netaddr4_make
 {a,b,c,d:nat | a <= 255; b <= 255; c <= 255; d <= 255}
 {p:nat | p <= 65535}
 (a: int(a), b: int(b), c: int(c), d: int(d), port: int(p)): netaddr4 = "mac#ats2_net_addr4_make"
fun netaddr4_loopback {p:nat | p <= 65535}(port: int(p)): netaddr4 = "mac#ats2_net_addr4_loopback"
fun netaddr4_port(a: &netaddr4 >> netaddr4): [p:nat | p <= 65535] int(p) = "mac#ats2_net_addr4_get_port"

fun udp4_new(): NetSock0(0, 0) = "mac#ats2_net_owned_udp4_open"
fun tcp4_new(): NetSock0(1, 0) = "mac#ats2_net_owned_tcp4_open"
fun netsock_isnot_null {l:addr}{p,q:int}(s: !netsock_vtype(l,p,q)):<> bool(l > null) = "mac#ats2_net_owned_isnot_null"
praxi netsock_free_null {p,q:int}(s: netsock_vtype(null,p,q)): void

fun udp_bind4 {l:addr | l > null}
 (s: !netsock_vtype(l,0,0) >> netsock_vtype(l,0,k), a: &netaddr4 >> netaddr4, err: &int? >> int): #[k:int | k == 0 || k == 1] int(k) = "mac#ats2_net_owned_udp_bind4_state"
fun tcp_bind4 {l:addr | l > null}
 (s: !netsock_vtype(l,1,0) >> netsock_vtype(l,1,k), a: &netaddr4 >> netaddr4, err: &int? >> int): #[k:int | k == 0 || k == 1] int(k) = "mac#ats2_net_owned_tcp_bind4_state"
fun tcp_listen {l:addr | l > null}
 (s: !netsock_vtype(l,1,1) >> netsock_vtype(l,1,k), backlog: int, err: &int? >> int): #[k:int | k == 1 || k == 2] int(k) = "mac#ats2_net_owned_tcp_listen_state"
fun tcp_connect4 {l:addr | l > null}
 (s: !netsock_vtype(l,1,0) >> netsock_vtype(l,1,k), a: &netaddr4 >> netaddr4, err: &int? >> int): #[k:int | k == 0 || k == 3] int(k) = "mac#ats2_net_owned_tcp_connect4_state"
fun tcp_accept {l:addr | l > null}
 (s: !netsock_vtype(l,1,2), err: &int? >> int): NetSock0(1,3) = "mac#ats2_net_owned_tcp_accept"
fun net_local4 {l:addr | l>null}{p,q:int | q >= 0}
 (s: !netsock_vtype(l,p,q), a: &netaddr4? >> netaddr4, err: &int? >> int): int = "mac#ats2_net_owned_local4"
fun tcp_send {l:addr | l>null}{n,m:nat | m <= n}
 (s: !netsock_vtype(l,1,3), buf: &(@[char][n]), count: size_t(m), err: &int? >> int)
 : [r:int | ~1 <= r; r <= m] ssize_t(r) = "mac#ats2_net_send"
fun tcp_recv {l:addr | l>null}{n,m:nat | m <= n}
 (s: !netsock_vtype(l,1,3), buf: &(@[char?][n]) >> _, count: size_t(m), err: &int? >> int)
 : [r:int | ~1 <= r; r <= m] ssize_t(r) = "mac#ats2_net_recv"
fun udp_sendto {l:addr | l>null}{q:int | q == 0 || q == 1}{n,m:nat | m <= n}
 (s: !netsock_vtype(l,0,q), buf: &(@[char][n]), count: size_t(m), a: &netaddr4 >> netaddr4, err: &int? >> int)
 : [r:int | ~1 <= r; r <= m] ssize_t(r) = "mac#ats2_net_sendto4"
fun udp_recvfrom {l:addr | l>null}{n,m:nat | m <= n}
 (s: !netsock_vtype(l,0,1), buf: &(@[char?][n]) >> _, count: size_t(m), a: &netaddr4? >> netaddr4, err: &int? >> int)
 : [r:int | ~1 <= r; r <= m] ssize_t(r) = "mac#ats2_net_owned_udp_recvfrom4"
fun netsock_try_close {l:addr | l>null}{p,q:int | q >= 0}
 (s: !netsock_vtype(l,p,q) >> netsock_vtype(l,p,k), state: int(q), err: &int? >> int)
 : #[k:int | k == ~1 || k == q] int(k) = "mac#ats2_net_owned_try_close"
fun netsock_free_closed {l:addr | l>null}{p:int}(s: netsock_vtype(l,p,~1)): void = "mac#ats2_net_owned_free_closed"
fun netsock_close_exn {l:addr | l > null}{p,q:int | q >= 0}
 (s: netsock_vtype(l,p,q)): void = "mac#ats2_net_owned_close_exn"

fun net_set_recv_timeout {l:addr | l>null}{p,q:int | q >= 0}
 (s: !netsock_vtype(l,p,q), ms: uint, err: &int? >> int): int = "mac#ats2_net_set_recv_timeout_ms"
