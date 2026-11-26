#include "share/atspre_staload.hats"
staload NET = "compat/ats2/SATS/net_socket_typed.sats"
implement main0 () = let
  val sock = $NET.tcp4_new()
  val ok = $NET.netsock_isnot_null(sock)
  val () = if ok then let
    var err: int = 0
    var msg = @[char]('x')
    val _ = $NET.tcp_send(sock, msg, i2sz(1), err)
    val () = $NET.netsock_close_exn(sock)
  in () end else let
    prval () = $NET.netsock_free_null(sock)
  in () end
in () end
