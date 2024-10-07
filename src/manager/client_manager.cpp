#include "client_manager.hpp"

#include <iostream>
#include <string>

auto ClientRead(Client &client, Client_State &state) -> void {
  auto tcp_recv = new TcpRecv{};
  tcp_recv->recv(client.socket, client.recv_buf);

  state = Client_State::WRITE;
}

auto ClientWrite(Client &client, Client_State &state) -> void {
  if (client.recv_buf[0] != 0) {
    std::cout << "server :" << client.recv_buf.data() << "\n";
  }
  auto tcp_send = new TcpSend{};
  auto str = std::string{};
  std::getline(std::cin, str);
  memset(client.send_buf.data(), 0, client.send_buf.size());
  memcpy(client.send_buf.data(), str.data(), str.length());

  tcp_send->send(client.socket, client.send_buf);
  state = Client_State::READ;
}
