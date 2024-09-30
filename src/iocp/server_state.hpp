#pragma once

#include <vector>
#include <unordered_map>

#include "iocp.hpp"

enum class Server_State {
  NONE,
  READ,
  WRITE,
  DISCONNECT,
};

struct Client {
  SOCKET socket;
  Server_State state;
  std::vector<char> c_buf = std::vector<char>(1000, 0);
};

struct Clients {
  HANDLE iocp_handle;
  std::unordered_map<SOCKET, Client> clients;
};

auto AddClient(Clients& clients, SOCKET socket) -> void;
auto Broadcast(Clients &clients, SOCKET sender_socket, const std::vector<char> &message) -> void;
auto ServerRead(Client &client, Clients &clients) -> void;
auto ServerWrite(Client &client, HANDLE iocp_handle) -> void;
auto ServerDisconnect(Client& client, SOCKET socket) -> void;
auto ServerIo(Clients &clients, SOCKET socket, DWORD bytes_transferred, LPOVERLAPPED overlapped) -> void;
