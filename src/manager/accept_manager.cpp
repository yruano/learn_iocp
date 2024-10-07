#include "accept_manager.hpp"

#include <format>

AcceptManager::AcceptManager(HANDLE iocp_handle, SOCKET listen_socket,
                             LPFN_ACCEPTEX fnAcceptEx)
    : iocp_handle_(iocp_handle), listen_socket_(listen_socket),
      fnAcceptEx_(fnAcceptEx) {}

AcceptManager::~AcceptManager() {}

bool AcceptManager::start_accept() {
  SOCKET accept_socket = create_tcp_socket(iocp_handle_);
  if (accept_socket == INVALID_SOCKET) {
    return false;
  }

  post_accept(listen_socket_, accept_socket);
  return true;
}

void AcceptManager::post_accept(SOCKET listen_socket, SOCKET accept_socket) {
  auto a = new TcpAccept{};
  a->ov.accept_socket = accept_socket;
  a->accept(listen_socket, accept_socket, fnAcceptEx_);
}

void AcceptManager::handle_accept(DWORD bytes_transferred, LPOVERLAPPED overlapped, int size) {
  auto ovex = std::bit_cast<OverlappedEx *>(overlapped);
  SOCKET accept_socket = ovex->accept_socket;

  postCustomMsg(iocp_handle_, accept_socket, Iotype::QUEUE);
  if (size < 5) {
    // 다음 클라이언트 수락 준비
    start_accept();
  }
}
