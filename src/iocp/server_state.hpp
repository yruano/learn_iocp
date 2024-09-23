#pragma once

#include <vector>
#include <unordered_map>

#include <windows.h>


enum class Server_State {
  NONE,
  READ,
  WRITE,
  DISCONNECT,
};

struct Client {
  SOCKET socket;
  std::vector<char> c_buf = std::vector<char>(1000, 0);
  Server_State state;
};

struct Clients {
  HANDLE iocp_handle;
  std::unordered_map<SOCKET, Client> clients;
  SOCKET socket;
};

// auto None() -> void;
auto ServerRead(Clients clients) -> void;
auto ServerWrite(Clients clients) -> void;
// auto ServerDisconnect() -> void;
