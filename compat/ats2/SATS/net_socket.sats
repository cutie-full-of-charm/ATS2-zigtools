%{#
#include "ats2_net_socket.h"
%}

absvtype net_socket_vtype(l:addr, p:int) = ptr
vtypedef net_socket(l:addr, p:int) = net_socket_vtype(l, p)
vtypedef NetSocket0(p:int) = [l:addr | l >= null] net_socket(l, p)
vtypedef NetSocket1(p:int) = [l:addr | l > null] net_socket(l, p)

fun udp4_open(): NetSocket0(0) = "mac#ats2_net_owned_udp4_open"
fun tcp4_open(): NetSocket0(1) = "mac#ats2_net_owned_tcp4_open"
fun socket_isnot_null {l:addr}{p:int}(s: !net_socket(l, p)):<> bool(l > null) = "mac#ats2_net_owned_isnot_null"
fun socket_close {l:addr | l > null}{p:int}(s: net_socket(l, p)): void = "mac#ats2_net_owned_close_exn"
praxi socket_free_null {p:int}(s: net_socket(null, p)): void
