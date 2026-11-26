#include "share/atspre_staload.hats"
staload UN = "libats/libc/SATS/unistd.sats"

implement main0 () = {
  val rc = $UN.chdir(".")
  val () = assertloc(rc = 0)
  val () = println!("unistd ATS ok")
}
