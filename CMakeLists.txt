set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

function(use_sanitizer target)
  set(USE_SANITIZER OFF CACHE BOOL "Use Sanitizer")
  if (USE_SANITIZER)
    # clang
    if ((CMAKE_CXX_COMPILER_ID MATCHES "Clang"))
      if (LINUX OR APPLE OR WIN32)
        set(CMAKE_CXX_FLAGS_DEBUG "-O1")
        set_property(TARGET ${target} PROPERTY MSVC_RUNTIME_LIBRARY MultiThreaded)
        set(sanitizer_opts -fno-omit-frame-pointer -fno-sanitize-recover=all -fsanitize=address,undefined)
      endif()
    endif()

    # gcc
    if (CMAKE_CXX_COMPILER_ID MATCHES "GNU")
      if (LINUX OR APPLE)
        set(CMAKE_CXX_FLAGS_DEBUG "-O1")
        set(sanitizer_opts -fno-omit-frame-pointer -fno-sanitize-recover=all -fsanitize=address,undefined)
      endif()
    endif()

    # msvc
    if (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
      set(CMAKE_CXX_FLAGS_DEBUG "-O1")
      get_property(msvc_rt TARGET ${target} PROPERTY MSVC_RUNTIME_LIBRARY)
      if (NOT DEFINED msvc_rt)
        set_property(TARGET ${target} PROPERTY MSVC_RUNTIME_LIBRARY MultiThreaded$<$<CONFIG:Debug>:Debug>DLL)
      else()
        string(REPLACE "Debug" "" msvc_rt ${msvc_rt})
        set_property(TARGET ${target} PROPERTY MSVC_RUNTIME_LIBRARY ${msvc_rt})
      endif()
      set(sanitizer_opts /fsanitize=address /Zi)
    endif()

    if (DEFINED sanitizer_opts)
      target_compile_options(${target} PRIVATE ${sanitizer_opts})
      target_link_options(${target} PRIVATE ${sanitizer_opts})
    endif()
  endif()
endfunction()

cmake_minimum_required(VERSION 3.0)
cmake_policy(SET CMP0048 NEW)

# 프로젝트 이름과 버전 설정
project(Example VERSION 1.0)

# C++ 표준 설정
set(CMAKE_CXX_STANDARD_REQUIRED True)

# 소스 파일 설정
set(SERVER_SOURCES
  src/server/server.cpp
  src/defer.hpp
  src/iocp/iocp.hpp
  src/iocp/iocp.cpp
  src/iocp/server_state.hpp
  src/iocp/server_state.cpp
  src/iocp/accept_manager.hpp
  src/iocp/accept_manager.cpp
)

set(CLIENT_SOURCES
  src/client/client.cpp
  src/defer.hpp 
  src/iocp/iocp.hpp
  src/iocp/iocp.cpp
  src/iocp/client_state.hpp
  src/iocp/client_state.cpp
)

# 서버 실행 파일 생성
add_executable(IOCPServer ${SERVER_SOURCES})
use_sanitizer(IOCPServer)
target_link_libraries(IOCPServer 
  ws2_32)

# 클라이언트 실행 파일 생성
add_executable(IOCPClient ${CLIENT_SOURCES})
use_sanitizer(IOCPClient)
target_link_libraries(IOCPClient 
  ws2_32)

set_property(TARGET IOCPServer PROPERTY CXX_STANDARD 20)
set_property(TARGET IOCPClient PROPERTY CXX_STANDARD 20)
