#include "share/atspre_staload.hats"
staload ST = "libats/libc/SATS/string.sats"
implement main0 () = {
  val msg = $ST.strerror_r_gc(2)
  val () = assertloc(strptr_length(msg) > 0)
  val () = println!(msg)
  val () = strptr_free(msg)
  val () = println!("strerror smoke ok")
}
