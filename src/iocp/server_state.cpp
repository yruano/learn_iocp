#include "server_state.hpp"

#include <iostream>

auto ServerRead(Client &client) -> void {
  std::cout << "Server_State : READ\n";
  client.state = Server_State::WRITE;
  auto r = new TcpRecv{};
  r->recv(client.socket, client.c_buf);
}

auto ServerWrite(Client &client, HANDLE iocp_handle) -> void {
  std::cout << "Server_State : WRITE\n";
  auto se = new TcpSend{};
  if (strcmp(client.c_buf.data(), "exit") == 0) {
    client.state = Server_State::DISCONNECT;
    if (!se->send(client.socket, client.c_buf)) {
      client.state = Server_State::DISCONNECT;
      postCustomMsg(iocp_handle, client.socket, Iotype::QUEUE);
    }
  } else {
    client.state = Server_State::READ;
    if (!se->send(client.socket, client.c_buf)) {
      client.state = Server_State::DISCONNECT;
      postCustomMsg(iocp_handle, client.socket, Iotype::QUEUE);
    }
  }
}
