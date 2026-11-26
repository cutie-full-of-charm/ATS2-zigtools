#include "share/atspre_staload.hats"
staload NET = "compat/ats2/SATS/net_socket_typed.sats"
implement main0 () = let
  val sock = $NET.udp4_new()
  val ok = $NET.netsock_isnot_null(sock)
  val () = if ok then let
    var err: int = 0
    val result = $NET.netsock_try_close(sock, 0, err)
    val () = if result = ~1 then let
      var endpoint: $NET.netaddr4?
      val _ = $NET.net_local4(sock, endpoint, err)
      val () = $NET.netsock_free_closed(sock)
    in () end else $NET.netsock_close_exn(sock)
  in () end else let
    prval () = $NET.netsock_free_null(sock)
  in () end
in () end
