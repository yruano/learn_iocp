#include "iocp.hpp"

#include <format>
#include <iostream>

#include <ws2tcpip.h>

IoOperation::~IoOperation() {}

EventLoopMsg::EventLoopMsg() {
  ov.op = this;
}

TcpAccept::TcpAccept() { 
  ov.op = this;
  ov.operation = STATE_READ;
}

TcpAccept::~TcpAccept() {
  std::cout << "Accept done\n";
}

auto TcpAccept::accept(SOCKET listen_socket, SOCKET accept_socket, LPFN_ACCEPTEX fnAcceptEx) -> bool {
  ov.op = this;
  ov.callback = callback;

  auto accept_success = fnAcceptEx(listen_socket, accept_socket, addr_buf, 0, sizeof(addr_buf) / 2,
                                   sizeof(addr_buf) / 2, &bytes_received, &ov);
  if (not accept_success) {
    auto err_code = ::WSAGetLastError();
    if (err_code != WSA_IO_PENDING) {
      std::cerr << std::format("AcceptEx failed: {}\n", err_code)
                << std::format("err msg: {}\n", std::system_category().message((int)err_code));
      ::closesocket(accept_socket);
      return false;
    }
  }
  return true;
}

TcpConnect::TcpConnect() {
  ov.op = this;
  ov.operation = STATE_READ;
}

TcpConnect::~TcpConnect() {
  std::cout << "Connect done\n";
}

auto TcpConnect::connect(SOCKET socket, LPFN_CONNECTEX fnConnectEx, const char *ip, std::int16_t port) -> bool {
  ov.op = this;
  ov.callback = callback;
  ov.operation = STATE_READ;

  // // resolve the server address and port
  // // https://stackoverflow.com/questions/21797913/purpose-of-linked-list-in-struct-addrinfo-in-socket-programming
  // auto conn_addrinfo = PADDRINFOA{};
  // auto addr_hints = addrinfo{};
  // addr_hints.ai_family = AF_INET;
  // addr_hints.ai_socktype = SOCK_STREAM;
  // addr_hints.ai_protocol = IPPROTO_TCP;
  // auto getaddr_result = ::getaddrinfo(ip, port, &addr_hints, &conn_addrinfo);
  // if (getaddr_result != 0) {
  //   std::cerr << std::format("getaddrinfo failed: {}\n", getaddr_result)
  //             << std::format("err msg: {}\n", ::gai_strerrorA(getaddr_result));
  //   ::closesocket(conn_socket);
  //   return EXIT_FAILURE;
  // }
  //
  // auto connect_success = fnConnectEx(conn_socket, conn_addrinfo->ai_addr, sizeof(SOCKADDR), nullptr, 0, nullptr, &ov);
  // ::freeaddrinfo(conn_addrinfo);

  conn_addr.sin_family = AF_INET;
  conn_addr.sin_port = ::htons(port);
  ::inet_pton(AF_INET, ip, &conn_addr.sin_addr);

  // 접속
  auto connect_success = fnConnectEx(socket, (SOCKADDR *)&conn_addr, sizeof(conn_addr), nullptr, 0, nullptr, &ov);
  if (not connect_success) {
    auto err_code = ::WSAGetLastError();
    if (err_code != WSA_IO_PENDING) {
      std::cerr << std::format("ConnectEx failed: {}\n", err_code)
                << std::format("err msg: {}\n", std::system_category().message((int)err_code));
      return false;
    }
  }
  return true;
}

TcpSend::TcpSend() {
  ov.op = this;
}

TcpSend::~TcpSend() {
  std::cout << "Send done\n";
}

auto TcpSend::send(SOCKET socket, std::span<const char> data) -> bool {
  wsa_buf.buf = const_cast<char *>(data.data());
  wsa_buf.len = data.size();
  ov.op = this;
  ov.operation = STATE_WRITE;
  
  std::format("현재 상태: {}", static_cast<int>(ov.operation));

  if (::WSASend(socket, &wsa_buf, 1, &bytes_sent, 0, &ov, nullptr) != 0) {
    auto err_code = ::WSAGetLastError();
    if (err_code != WSA_IO_PENDING) {
      std::cerr << std::format("WSASend failed: {}\n", err_code)
                << std::format("err msg: {}\n", std::system_category().message((int)err_code));
      return false;
    }
  }
  return true;
}

TcpRecv::TcpRecv() {
  ov.op = this;
}

TcpRecv::~TcpRecv() {
  std::cout << "Recv done: " << wsa_buf.buf << '\n';
}

auto TcpRecv::recv(SOCKET socket, std::span<char> buf) -> bool {
  wsa_buf.buf = buf.data();
  wsa_buf.len = buf.size();
  ov.op = this;
  ov.operation = STATE_WRITE;
 
  std::format("현재 상태: {}", static_cast<int>(ov.operation));
  
  if (::WSARecv(socket, &wsa_buf, 1, &bytes_recv, &flags, &ov, nullptr) != 0) {
    auto err_code = ::WSAGetLastError();
    if (err_code != WSA_IO_PENDING) {
      std::cerr << std::format("WSARecv failed: {}\n", err_code)
                << std::format("err msg: {}\n", std::system_category().message((int)err_code));
      return false;
    }
  }
  return true;
}

auto create_iocp_handle() -> HANDLE {
  auto iocp_handle = ::CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);
  if (iocp_handle == nullptr) {
    auto err_code = ::GetLastError();
    std::cerr << std::format("CreateIoCompletionPort failed: {}\n", err_code)
              << std::format("err msg: {}\n", std::system_category().message((int)err_code));
    return nullptr;
  }
  return iocp_handle;
}

auto close_iocp_handle(HANDLE iocp_handle) -> void {
  if (::CloseHandle(iocp_handle) == 0) {
    auto err_code = ::GetLastError();
    std::cerr << std::format("CloseHandle failed: {}\n", err_code)
              << std::format("err msg: {}\n", std::system_category().message((int)err_code));
  }
}

auto create_tcp_socket(HANDLE iocp_handle) -> SOCKET {
  // af:
  // IPv4 = AF_INET
  // IPv6 = AF_INET6

  // type, protocol:
  // TCP = SOCK_STREAM, IPPROTO_TCP
  // UDP = SOCK_DGRAM, IPPROTO_UDP

  // https://yamoe.tistory.com/423
  // https://tomson0119.tistory.com/6

  auto socket = ::WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_OVERLAPPED);
  if (socket == INVALID_SOCKET) {
    auto err_code = ::WSAGetLastError();
    std::cerr << std::format("WSASocketW failed: {}\n", err_code)
              << std::format("err msg: {}\n", std::system_category().message((int)err_code));
    return INVALID_SOCKET;
  }

  // setup IOCP
  if (not ::CreateIoCompletionPort(std::bit_cast<HANDLE>(socket), iocp_handle, (ULONG_PTR)0, 0)) {
    auto err_code = ::GetLastError();
    std::cerr << std::format("CreateIoCompletionPort failed: {}\n", err_code)
              << std::format("err msg: {}\n", std::system_category().message((int)err_code));
    ::closesocket(socket);
    return INVALID_SOCKET;
  }

  return socket;
}

auto load_fn_acceptex(SOCKET socket) -> LPFN_ACCEPTEX {
  auto fnAcceptEx = LPFN_ACCEPTEX{nullptr};
  auto guid = GUID WSAID_ACCEPTEX;
  auto bytes = DWORD{};
  auto load_fn_result = ::WSAIoctl(socket, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid, sizeof(guid), &fnAcceptEx,
                                   sizeof(fnAcceptEx), &bytes, nullptr, nullptr);
  if (load_fn_result != 0) {
    auto err_code = ::WSAGetLastError();
    std::cerr << std::format("AcceptEx load failed: {}\n", err_code)
              << std::format("err msg: {}\n", std::system_category().message((int)err_code));
    return nullptr;
  }
  return fnAcceptEx;
}

auto load_fn_connectex(SOCKET socket) -> LPFN_CONNECTEX {
  auto fnConnectEx = LPFN_CONNECTEX{nullptr};
  auto guid = GUID WSAID_CONNECTEX;
  auto bytes = DWORD{};
  auto load_fn_result = ::WSAIoctl(socket, SIO_GET_EXTENSION_FUNCTION_POINTER, &guid, sizeof(guid), &fnConnectEx,
                                   sizeof(fnConnectEx), &bytes, nullptr, nullptr);
  if (load_fn_result != 0) {
    auto err_code = ::WSAGetLastError();
    std::cerr << std::format("ConnectEx load failed: {}\n", err_code)
              << std::format("err msg: {}\n", std::system_category().message((int)err_code));
    return nullptr;
  }
  return fnConnectEx;
}
