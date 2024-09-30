#include <cstdlib>
#include <format>
#include <iostream>

#include "../defer.hpp"
#include "../iocp/iocp.hpp"
#include "../iocp/client_state.hpp"

constexpr auto ip = "127.0.0.1"; // localhost
constexpr auto port = 3000;

auto run_client = true;
auto state = Client_State{};
auto iocp_handle = HANDLE{nullptr};

auto main() -> int {
  std::cout << "client start\n";
  
  auto client = Client{};

  // WSA 초기화
  auto wsa_data = WSADATA{};
  auto startup_result = ::WSAStartup(MAKEWORD(2, 2), &wsa_data);
  if (startup_result != 0) {
    std::cerr << std::format("WSAStartup failed: {}\n", startup_result)
              << std::format("err msg: {}\n", std::system_category().message((int)startup_result));
  }

  // WSA 초기화 해제
  defer([]() {
    std::cout << "client closed\n";
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
        state = Client_State::DISCONNECT;
        auto ovex = new OverlappedEx{};
        ::PostQueuedCompletionStatus(iocp_handle, 0, 0, ovex);
      }
      return true;
    },
    true);

  // 접속 소켓 생성
  client.socket = create_tcp_socket(iocp_handle);
  if (client.socket == INVALID_SOCKET) {
    return EXIT_FAILURE;
  }

  // 접속 소켓 닫기
  defer([=]() {
    ::closesocket(client.socket);
  });

  // ConnectEx 함수 포인터 불러오기
  auto fnConnectEx = load_fn_connectex(client.socket);
  if (fnConnectEx == nullptr) {
    return EXIT_FAILURE;
  }

  // bind
  auto addr = SOCKADDR_IN{};
  addr.sin_family = AF_INET;
  addr.sin_addr.S_un.S_addr = ::htonl(INADDR_ANY);
  addr.sin_port = ::htons(0);

  if (::bind(client.socket, (SOCKADDR *)&addr, sizeof(addr)) != 0) {
    auto err_code = ::WSAGetLastError();
    std::cerr << std::format("socket bind failed: {}\n", err_code)
              << std::format("err msg: {}\n", std::system_category().message((int)err_code));
    return EXIT_FAILURE;
  }

  auto c = new TcpConnect{};
  c->connect(client.socket, fnConnectEx, ip, port);
  state = Client_State::WRITE;

  // IOCP가 완료되면 로직 수행 (이벤트 루프)
  while (run_client) {
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
        state = Client_State::DISCONNECT;
        auto ovex = new OverlappedEx{};
        ::PostQueuedCompletionStatus(iocp_handle, 0, 0, ovex);
        continue;
      }
    }

    switch (state) {
      case Client_State::READ: {
        ClientRead(client, state);
        break;
      }
      case Client_State::WRITE: {
        ClientWrite(client, state);
        break;
      }
      case Client_State::CONNECT:
        break;
      
      case Client_State::DISCONNECT:
        closesocket(client.socket);
        run_client = false;
        break;
      
      default:
        break;
    }
    // IOCP 완료됨
    auto ovex = std::bit_cast<OverlappedEx *>(ov);
    delete ovex->op;
  }

  return EXIT_SUCCESS;
}
