#include "share/atspre_staload.hats"
staload SK = "libats/SATS/linmap_skiplist.sats"

implement main0 () = let
  val () = $SK.linmap_skiplist_initize()
in
  println!("skiplist init ok")
end
