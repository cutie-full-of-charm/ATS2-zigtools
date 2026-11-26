#include "share/atspre_staload.hats"
staload NET = "compat/ats2/SATS/net_socket.sats"
implement main0() = let
  val sock = $NET.udp4_open()
  val valid = $NET.socket_isnot_null(sock)
in
  if valid then let
    val () = $NET.socket_close(sock)
    val () = $NET.socket_close(sock)
  in
    ()
  end else let
    prval () = $NET.socket_free_null(sock)
  in
    ()
  end
end
