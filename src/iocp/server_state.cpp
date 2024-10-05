#include "server_state.hpp"

#include <iostream>
#include <format>
// 클라이언트 추가 함수
auto AddClient(Clients &clients, SOCKET socket) -> void {
  clients.clients.insert({socket, {socket, Server_State::READ}});
}

// 클라이언트 제거 함수
auto ServerDisconnect(Clients &clients, SOCKET socket) -> void {
  auto it = clients.clients.find(socket);
  if (it != clients.clients.end()) {
    ::closesocket(it->first);
    clients.clients.erase(it);
  }
}

// TcpRecv이런 사용할 때는 무조건 new를 사용해야한다 실행이 끝날때 까지 사라있어야함
auto ServerRead(Client &client, Clients &clients) -> void {
  client.state = Server_State::WRITE;
  auto r = new TcpRecv{};
  r->recv(client.socket, client.c_buf);
}

auto ServerWrite(Client &sender_client, Clients &clients, HANDLE iocp_handle) -> void {
  std::cout << "Server_State : WRITE\n";
  if (strcmp(sender_client.c_buf.data(), "exit") == 0) {
    sender_client.state = Server_State::DISCONNECT;
    for (auto &[socket, client] : clients.clients) {
      if (client.socket != sender_client.socket) {
        auto sender = new TcpSend{};
        if (!sender->send(client.socket, sender_client.c_buf)) {
          auto err_code = ::WSAGetLastError();
          std::cerr << std::format("err msg: {}\n", std::system_category().message((int)err_code));
        }
      }
    }
  } else {
    sender_client.state = Server_State::READ;
    for (auto &[socket, client] : clients.clients) {
      if (client.socket != sender_client.socket) {
        auto sender = new TcpSend{};
        if (!sender->send(client.socket, sender_client.c_buf)) {
          auto err_code = ::WSAGetLastError();
          std::cerr << std::format("err msg: {}\n", std::system_category().message((int)err_code));
        }
      }
    }
  }
}

// 클라이언트 I/O 처리 함수
auto ServerIo(Clients &clients, SOCKET socket, DWORD bytes_transferred, LPOVERLAPPED overlapped) -> void {
  auto &client = clients.clients[socket];
  switch (client.state) {
  case Server_State::READ:
    ServerRead(client, clients);
    break;
  case Server_State::WRITE:
    ServerWrite(client, clients, clients.iocp_handle);
    break;
  case Server_State::DISCONNECT:
    ServerDisconnect(clients, socket);
    break;
  default:
    break;
  }
}
