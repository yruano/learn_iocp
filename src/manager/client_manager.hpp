#pragma once

#include "../iocp/iocp.hpp"
#include <vector>

enum class Client_State {
  CONNECT,
  READ,
  WRITE,
  DISCONNECT
};

struct Client {
  SOCKET socket;
  std::vector<char> recv_buf = std::vector<char>(1000, 0);
  std::vector<char> send_buf = std::vector<char>(1000, 0);
};

auto ClientRead(Client &client, Client_State &state) -> void;
auto ClientWrite(Client &client, Client_State &state) -> void;
