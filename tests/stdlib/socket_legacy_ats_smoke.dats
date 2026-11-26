#include "share/atspre_define.hats"
#include "share/atspre_staload.hats"

staload SOCKET = "libats/libc/SATS/sys/socket.sats"

implement main0 () = {
  val (pf | fd) = $SOCKET.socket_AF_type_exn($SOCKET.AF_INET, $SOCKET.SOCK_STREAM)
  val () = $SOCKET.socket_close_exn(pf | fd)
  val () = println!("socket legacy ATS ok")
}
