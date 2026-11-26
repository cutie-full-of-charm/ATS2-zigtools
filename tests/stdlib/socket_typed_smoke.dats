#include "share/atspre_staload.hats"
staload NET = "compat/ats2/SATS/net_socket_typed.sats"

fun finish {l:addr | l>null}{p,q:int | q >= 0}
  (s: $NET.netsock_vtype(l,p,q), state: int(q)): void = let
  var err: int = 0
  val outcome = $NET.netsock_try_close(s, state, err)
  val () = if outcome = ~1 then $NET.netsock_free_closed(s) else $NET.netsock_close_exn(s)
in () end

fun tcp_exercise {l:addr | l>null}
  (server: !($NET.netsock_vtype(l,1,2)), endpoint: &$NET.netaddr4 >> $NET.netaddr4): void = let
  var err: int = 0
  val client = $NET.tcp4_new()
  val client_ok = $NET.netsock_isnot_null(client)
  val () = if client_ok then let
    val state = $NET.tcp_connect4(client, endpoint, err)
    val () = if state = 3 then let
      val accepted = $NET.tcp_accept(server, err)
      val accepted_ok = $NET.netsock_isnot_null(accepted)
      val () = if accepted_ok then let
        val () = assertloc($NET.net_set_recv_timeout(accepted, 2000u, err) = 0)
        var tx = @[char]('T', 'C', 'P')
        val sent = $NET.tcp_send(client, tx, i2sz(3), err)
        val () = assertloc(sent > 0)
        var rx = @[char][8]()
        val received = $NET.tcp_recv(accepted, rx, i2sz(8), err)
        val () = assertloc(received > 0)
        val () = finish(accepted, 3)
        val () = finish(client, 3)
      in () end else let
        prval () = $NET.netsock_free_null(accepted)
        val () = finish(client, 3)
        val () = assertloc(false)
      in () end
    in () end else let
      val () = finish(client, 0)
      val () = assertloc(false)
    in () end
  in () end else let
    prval () = $NET.netsock_free_null(client)
    val () = assertloc(false)
  in () end
in () end

implement main0() = let
  var err: int = 0
  var addr = $NET.netaddr4_make(127, 0, 0, 1, 0)
  val udp = $NET.udp4_new()
  val udp_ok = $NET.netsock_isnot_null(udp)
  val () = if udp_ok then let
    val state = $NET.udp_bind4(udp, addr, err)
    val () = if state = 1 then let
      var endpoint: $NET.netaddr4?
      val () = assertloc($NET.net_local4(udp, endpoint, err) = 0)
      val () = assertloc($NET.net_set_recv_timeout(udp, 2000u, err) = 0)
      var tx = @[char]('p', 'i', 'n', 'g')
      val sent = $NET.udp_sendto(udp, tx, i2sz(4), endpoint, err)
      val () = assertloc(sent = 4)
      var rx = @[char][8]()
      var peer: $NET.netaddr4?
      val received = $NET.udp_recvfrom(udp, rx, i2sz(8), peer, err)
      val () = assertloc(received = 4)
      val () = finish(udp, 1)
    in () end else let
      val () = finish(udp, 0)
      val () = assertloc(false)
    in () end
  in () end else let
    prval () = $NET.netsock_free_null(udp)
    val () = assertloc(false)
  in () end
  var tcp_addr = $NET.netaddr4_loopback(0)
  val tcp = $NET.tcp4_new()
  val tcp_ok = $NET.netsock_isnot_null(tcp)
  val () = if tcp_ok then let
    val bound = $NET.tcp_bind4(tcp, tcp_addr, err)
    val () = if bound = 1 then let
      val listening = $NET.tcp_listen(tcp, 4, err)
      val () = if listening = 2 then let
        var endpoint: $NET.netaddr4?
        val () = assertloc($NET.net_local4(tcp, endpoint, err) = 0)
        val () = tcp_exercise(tcp, endpoint)
        val () = finish(tcp, 2)
      in () end else finish(tcp, 1)
    in () end else finish(tcp, 0)
  in () end else let
    prval () = $NET.netsock_free_null(tcp)
    val () = assertloc(false)
  in () end
in
  println!("socket typed ok")
end
