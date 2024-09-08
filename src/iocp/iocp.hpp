#pragma once

#include <span>
#include <functional>
#include <cstdint>

#include <winsock2.h>
#include <mswsock.h>
#include <windows.h>

enum class Server_State {
  NONE,
  READ,
  WRITE,
  DISCONNECT,
};

enum class Program_State {
  RUN,
  EXIT
};

enum class Accept_State {
  RUN,
  EXIT
};

struct Client {
  SOCKET socket;
  std::vector<char> c_buf = std::vector<char>(1000, 0);
  Server_State state;
};

enum class Client_State {
  CONNECT,
  READ,
  WRITE,
  DISCONNECT
};

enum class Iotype {
  ACCPET,
  CONNECT,
  RECV,
  SEND,
  QUEUE
};

struct IoOperation {
  virtual ~IoOperation();
};

struct OverlappedEx : OVERLAPPED {
  IoOperation *op = nullptr;
  bool stop_event_loop = false;
  std::function<void()> callback = nullptr;
  Iotype iotype;
  SOCKET accept_socket;
};

struct EventLoopMsg : IoOperation {
  OverlappedEx ov{};
  EventLoopMsg();
};

struct TcpAccept : IoOperation {
  TcpAccept();
  alignas(8) std::uint8_t addr_buf[88]{};
  DWORD bytes_received{};
  OverlappedEx ov{};
  std::function<void()> callback = nullptr;

  ~TcpAccept();

  auto accept(SOCKET listen_socket, SOCKET accept_socket, LPFN_ACCEPTEX fnAcceptEx) -> bool;
};

struct TcpConnect : IoOperation {
  TcpConnect();
  SOCKADDR_IN conn_addr{};
  OverlappedEx ov{};
  std::function<void()> callback = nullptr;

  ~TcpConnect();

  auto connect(SOCKET socket, LPFN_CONNECTEX fnConnectEx, const char *ip, std::int16_t port) -> bool;
};

struct TcpSend : IoOperation {
  TcpSend();
  WSABUF wsa_buf{};
  DWORD bytes_sent{};
  OverlappedEx ov{};

  ~TcpSend();

  auto send(SOCKET socket, std::span<const char> data) -> bool;
};

struct TcpRecv : IoOperation {
  TcpRecv();
  WSABUF wsa_buf{};
  DWORD bytes_recv{};
  ULONG flags{};
  OverlappedEx ov{};

  ~TcpRecv();

  auto recv(SOCKET socket, std::span<char> buf) -> bool;
};

auto create_iocp_handle() -> HANDLE;
auto close_iocp_handle(HANDLE iocp_handle) -> void;

auto create_tcp_socket(HANDLE iocp_handle) -> SOCKET;
auto load_fn_acceptex(SOCKET socket) -> LPFN_ACCEPTEX;
auto load_fn_connectex(SOCKET socket) -> LPFN_CONNECTEX;
