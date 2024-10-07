#include "client_manager.hpp"
#include <algorithm>
#include <iostream>
#include <string>

auto clientRead(Client &client, Client_State &state) -> void {
  auto tcp_recv = new TcpRecv{};
  tcp_recv->recv(client.socket, client.recv_buf);

  state = Client_State::WRITE;
}

auto clientWrite(Client &client, Client_State &state) -> void {
  if (!client.recv_buf.empty() && client.recv_buf[0] != 0) {
    std::cout << "server: " << client.recv_buf.data() << "\n";
  }

  auto tcp_send = new TcpSend{};
  std::string str;
  std::getline(std::cin, str);

  // 버퍼 초기화 및 복사
  std::fill(client.send_buf.begin(), client.send_buf.end(), 0);
  std::copy(str.begin(), str.end(), client.send_buf.begin());

  tcp_send->send(client.socket, client.send_buf);
  state = Client_State::READ;
}

auto clientIo(Client_State state, Client &client, bool &run_client) -> void {
  switch (state) {
  case Client_State::READ: {
    clientRead(client, state);
    break;
  }
  case Client_State::WRITE: {
    clientWrite(client, state);
    break;
  }
  case Client_State::CONNECT:
    break;

  case Client_State::DISCONNECT:
    closesocket(client.socket);
    run_client = false;
    break;

  default:
    break;
  }
}
