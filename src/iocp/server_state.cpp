#include "server_state.hpp"
#include "iocp.hpp"

#include <iostream>

auto ServerRead(Clients clients) -> void {
  std::cout << "READ\n";
  clients.clients[clients.socket].state = Server_State::WRITE;
  auto r = new TcpRecv{};
  r->recv(clients.clients[clients.socket].socket,
          clients.clients[clients.socket].c_buf);
}

auto ServerWrite(Clients clients) -> void {
  std::cout << clients.clients[clients.socket].c_buf.data() << '\n';
  std::cout << "WRITE\n";
  auto se = new TcpSend{};
  if (strcmp(clients.clients[clients.socket].c_buf.data(), "exit") == 0) {
    clients.clients[clients.socket].state = Server_State::DISCONNECT;
    if (!se->send(clients.socket, clients.clients[clients.socket].c_buf)) {
      clients.clients[clients.socket].state = Server_State::DISCONNECT;
      postCustomMsg(clients.iocp_handle, socket, Iotype::QUEUE);
    }
  } else {
    clients.clients[clients.socket].state = Server_State::READ;
    if (!se->send(clients.socket, clients.clients[clients.socket].c_buf)) {
      clients.clients[clients.socket].state = Server_State::DISCONNECT;
      postCustomMsg(clients.iocp_handle, socket, Iotype::QUEUE);
    }
  }
}

// auto ServerDisconnect() -> void {}
