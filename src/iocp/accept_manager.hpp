#pragma once

#include "iocp.hpp"

class AcceptManager {
private:
  HANDLE iocp_handle_;
  SOCKET listen_socket_;
  LPFN_ACCEPTEX fnAcceptEx_;

  void post_accept(SOCKET listen_socket, SOCKET accept_socket);

public:
  AcceptManager(HANDLE iocp_handle, SOCKET listen_socket,
                LPFN_ACCEPTEX fnAcceptEx);
  ~AcceptManager();

  bool start_accept();
  void handle_accept(DWORD bytes_transferred, LPOVERLAPPED overlapped);
};
