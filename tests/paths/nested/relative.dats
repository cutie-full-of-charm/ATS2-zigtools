#include "share/atspre_staload.hats"
#include "./sibling.hats"
staload "./sibling.sats"
dynload "./sibling.dats"
implement main0 () = assertloc (path_test_value () = PATH_TEST_VALUE)
