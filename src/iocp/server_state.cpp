#include "server_state.hpp"

#include <iostream>

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

auto Broadcast(Clients &clients, SOCKET sender_socket, const std::vector<char> &message) -> void {
  for (auto &[socket, client] : clients.clients) {
    if (client.socket != sender_socket) {
      std::cout << "wow" << "\n";
      auto sender = TcpSend{};
      if (!sender.send(client.socket, message)) {
        std::cerr << "Failed to send message to socket: " << socket << "\n";
      } else {
        std::cout << "보내기 성공" << "\n";
      }
    }
  }
}

auto ServerRead(Client &client, Clients &clients) -> void {
  std::cout << "Server_State : READ\n";
  client.state = Server_State::WRITE;
  auto r = new TcpRecv{};
  r->recv(client.socket, client.c_buf);
  Broadcast(clients, client.socket, client.c_buf);
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

// 클라이언트 I/O 처리 함수
auto ServerIo(Clients &clients, SOCKET socket, DWORD bytes_transferred, LPOVERLAPPED overlapped) -> void {
  auto &client = clients.clients[socket];
  switch (client.state) {
  case Server_State::READ:
    ServerRead(client, clients);
    break;
  case Server_State::WRITE:
    ServerWrite(client, clients.iocp_handle);
    break;
  case Server_State::DISCONNECT:
    ServerDisconnect(clients, socket);
    break;
  default:
    break;
  }
}
