#include "share/atspre_staload.hats"
staload NET = "compat/ats2/SATS/net_socket_typed.sats"
implement main0 () = let
  val sock = $NET.udp4_new()
  val ok = $NET.netsock_isnot_null(sock)
  val () = if ok then let
    var err: int = 0
    var target = $NET.netaddr4_loopback(80)
    var msg = @[char]('a','b','c','d')
    val _ = $NET.udp_sendto(sock, msg, i2sz(5), target, err)
    val () = $NET.netsock_close_exn(sock)
  in () end else let
    prval () = $NET.netsock_free_null(sock)
  in () end
in () end
