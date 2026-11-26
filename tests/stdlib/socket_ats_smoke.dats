#include "share/atspre_staload.hats"
staload NET = "compat/ats2/SATS/net_socket.sats"
implement main0() = let
  val udp = $NET.udp4_open()
  val udp_valid = $NET.socket_isnot_null(udp)
  val () = if udp_valid then let
    val () = $NET.socket_close(udp)
  in () end else let
    prval () = $NET.socket_free_null(udp)
    val () = assertloc(false)
  in () end
  val tcp = $NET.tcp4_open()
  val tcp_valid = $NET.socket_isnot_null(tcp)
  val () = if tcp_valid then let
    val () = $NET.socket_close(tcp)
  in () end else let
    prval () = $NET.socket_free_null(tcp)
    val () = assertloc(false)
  in () end
in
  println!("socket ATS ok")
end
