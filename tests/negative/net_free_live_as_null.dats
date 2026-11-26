#include "share/atspre_staload.hats"
staload NET = "compat/ats2/SATS/net_socket_typed.sats"
implement main0 () = let
  val sock = $NET.udp4_new()
  val ok = $NET.netsock_isnot_null(sock)
  val () = if ok then let
    prval () = $NET.netsock_free_null(sock)
  in () end else let
    prval () = $NET.netsock_free_null(sock)
  in () end
in () end
