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

// auto None() -> void;
auto ServerRead(Client &clients) -> void;
auto ServerWrite(Client &clients, HANDLE iocp_handle) -> void;
// auto ServerDisconnect() -> void;
