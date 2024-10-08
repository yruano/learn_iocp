#include <cstdlib>
#include <format>
#include <iostream>

#include "../defer.hpp"
#include "../manager/accept_manager.hpp"
#include "../iocp/iocp.hpp"
#include "../manager/server_manager.hpp"

// IOCP 최적화 팁
// https://yamoe.tistory.com/421

// IOCP를 쓰레드에서 연결 해제 하는 법
// https://devblogs.microsoft.com/oldnewthing/20210120-00/?p=104740

constexpr auto port = 3000;

auto run_server = true;
auto program_state = Program_State::RUN;
auto accept_state = Accept_State::RUN;
auto pointer_map = std::unordered_map<std::string, void *>{};

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

  Clients clients = {};
  // IOCP 핸들 생성
  clients.iocp_handle = create_iocp_handle();
  pointer_map.insert({"iocp_handle", &clients.iocp_handle});
  if (clients.iocp_handle == nullptr) {
    return EXIT_FAILURE;
  }

  // IOCP 핸들 닫기
  defer([=]() {
    close_iocp_handle(clients.iocp_handle);
  })

    // ctrl-c 처리
    ::SetConsoleCtrlHandler(
      [](DWORD signal) -> BOOL {
        if (signal == CTRL_C_EVENT) {
          program_state = Program_State::EXIT;
          postCustomMsg(*(HANDLE *)pointer_map.at("iocp_handle"), 0, Iotype::QUEUE);
        }
        return true;
      },
      true);

  // listen 소켓 생성
  // https://github.com/microsoft/Windows-classic-samples/blob/main/Samples/Win7Samples/netds/winsock/iocp/server/IocpServer.Cpp#L373
  auto listen_socket = create_tcp_socket(clients.iocp_handle);
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

  AcceptManager accept_manager(clients.iocp_handle, listen_socket, fnAcceptEx);

  // 최초 accept 시작
  if (!accept_manager.start_accept()) {
    return EXIT_FAILURE;
  }

  // IOCP가 완료되면 로직 수행 (이벤트 루프)
  while (run_server) {
    auto bytes_transferred = DWORD{};
    auto compeletion_key = ULONG_PTR{};
    auto ov = LPOVERLAPPED{nullptr};

    // 하나의 구조체를 만들던 아니면 클래스를 하나 만들던 하면 좋을거 같긴하군
    if (not::GetQueuedCompletionStatus(clients.iocp_handle, &bytes_transferred, &compeletion_key, &ov, INFINITE)) {
      auto err_code = ::GetLastError();
      if (err_code == WAIT_TIMEOUT) {
        continue;
      } else {
        std::cerr << std::format("GetQueuedCompletionStatus failed: {}\n", err_code)
                  << std::format("err msg: {}\n", std::system_category().message((int)err_code));
        auto socket = std::bit_cast<SOCKET>(compeletion_key);
        clients.clients[socket].state = Server_State::DISCONNECT;
      }
    }

    auto ovex = std::bit_cast<OverlappedEx *>(ov);
    auto socket = std::bit_cast<SOCKET>(compeletion_key);

    std::cout << "Iotype: " << (int)ovex->iotype << "\n";
    std::cout << "Socket: " << (int)socket << "\n";

    switch (program_state) {
    case Program_State::RUN:
      if (ovex->iotype == Iotype::ACCPET) {
        AddClient(clients, ovex->accept_socket);
        accept_manager.handle_accept(bytes_transferred, ov, clients.clients.size());
      } else {
        ServerIo(clients, socket, bytes_transferred, ov);
      }
      break;
    case Program_State::EXIT:
      run_server = false;
      break;
    }

    delete ovex->op;
  }

  return EXIT_SUCCESS;
}
