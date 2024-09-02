#include <cstdlib>
#include <span>
#include <format>
#include <functional>
#include <iostream>
#include <string>

#include "../defer.hpp"
#include "../iocp/iocp.hpp"

// IOCP 최적화 팁
// https://yamoe.tistory.com/421

// IOCP를 쓰레드에서 연결 해제 하는 법
// https://devblogs.microsoft.com/oldnewthing/20210120-00/?p=104740

constexpr auto port = 3000;

auto iocp_handle = HANDLE{nullptr};

auto main() -> int {
  std::cout << "server start\n";

  // WSA 초기화
  auto wsa_data = WSADATA{};
  auto startup_result = ::WSAStartup(MAKEWORD(2, 2), &wsa_data);
  if (startup_result != 0) {
    std::cerr << std::format("WSAStartup failed: {}\n", startup_result)
              << std::format("err msg: {}\n", std::system_category().message((int)startup_result));
  }

  // WSA 초기화 해제
  defer([]() {
    std::cout << "server closed\n";
    auto cleanup_result = ::WSACleanup();
    if (cleanup_result != 0) {
      std::cerr << std::format("WSACleanup failed: {}\n", cleanup_result)
                << std::format("err msg: {}\n", std::system_category().message((int)cleanup_result));
    }
  });

  // IOCP 핸들 생성
  iocp_handle = create_iocp_handle();
  if (iocp_handle == nullptr) {
    return EXIT_FAILURE;
  }

  // IOCP 핸들 닫기
  defer([=]() {
    close_iocp_handle(iocp_handle);
  });

  // ctrl-c 처리
  ::SetConsoleCtrlHandler(
    [](DWORD signal) -> BOOL {
      if (signal == CTRL_C_EVENT) {
        auto op = new EventLoopMsg{};
        op->ov.stop_event_loop = true;
        ::PostQueuedCompletionStatus(iocp_handle, 0, 0, &op->ov);
      }
      return true;
    },
    true);

  // listen 소켓 생성
  // https://github.com/microsoft/Windows-classic-samples/blob/main/Samples/Win7Samples/netds/winsock/iocp/server/IocpServer.Cpp#L373
  auto listen_socket = create_tcp_socket(iocp_handle);
  if (listen_socket == INVALID_SOCKET) {
    return EXIT_FAILURE;
  }

  // listen 소켓 닫기
  defer([=]() {
    ::closesocket(listen_socket);
  });

  auto fnAcceptEx = load_fn_acceptex(listen_socket);
  if (fnAcceptEx == nullptr) {
    return EXIT_FAILURE;
  }

  // bind
  auto addr = SOCKADDR_IN{};
  addr.sin_family = AF_INET;
  addr.sin_addr.S_un.S_addr = ::htonl(INADDR_ANY);
  addr.sin_port = ::htons(port);

  if (::bind(listen_socket, (SOCKADDR *)&addr, sizeof(addr)) != 0) {
    auto err_code = ::WSAGetLastError();
    std::cerr << std::format("socket bind failed: {}\n", err_code)
              << std::format("err msg: {}\n", std::system_category().message((int)err_code));
    return EXIT_FAILURE;
  }

  // listen
  if (::listen(listen_socket, SOMAXCONN) != 0) {
    auto err_code = ::WSAGetLastError();
    std::cerr << std::format("listen failed: {}\n", err_code)
              << std::format("err msg: {}\n", std::system_category().message((int)err_code));
    return EXIT_FAILURE;
  }

  // accept socket 생성
  auto accept_socket = create_tcp_socket(iocp_handle);
  if (accept_socket == INVALID_SOCKET) {
    return EXIT_FAILURE;
  }

  // accept
  auto a = new TcpAccept{};
  a->callback = [=]() {
    auto s = new TcpSend{};
    s->send(accept_socket, "hello");
  };
  a->accept(listen_socket, accept_socket, fnAcceptEx);

  // client가 보넨 데이터를 저장
  auto str = std::vector<char>(100);
  // IOCP가 완료되면 로직 수행 (이벤트 루프)
  while (true) {
    auto bytes_transferred = DWORD{};
    auto compeletion_key = ULONG_PTR{};
    auto ov = LPOVERLAPPED{nullptr};

    if (not ::GetQueuedCompletionStatus(iocp_handle, &bytes_transferred, &compeletion_key, &ov, INFINITE)) {
      auto err_code = ::GetLastError();
      if (err_code == WAIT_TIMEOUT) {
        continue;
      } else {
        std::cerr << std::format("GetQueuedCompletionStatus failed: {}\n", err_code)
                  << std::format("err msg: {}\n", std::system_category().message((int)err_code));
        return EXIT_FAILURE;
      }
    }

    switch (a->ov.operation) {
      case STATE_READ: {
        auto r = new TcpRecv{};
        r->recv(accept_socket, str);
        break;
      }
      case STATE_WRITE: {
        auto se = new TcpSend{};
        se->send(accept_socket, str);
        break;
      }
      case STATE_ACCEPT:
        break;
      
      case STATE_DISCONNECT:
        break;
      
      default:
        break;
    }

    // IOCP 완료됨
    auto ovex = std::bit_cast<OverlappedEx *>(ov);
    auto stop_event_loop = ovex->stop_event_loop;
    if (ovex->callback) {
      ovex->callback();
    }
    delete ovex->op;

    if (stop_event_loop) {
      std::cout << "stop event loop\n";
      break;
    }
  }

  return EXIT_SUCCESS;
}
