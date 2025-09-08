cmake_minimum_required(VERSION 3.12)
project(lanchat_server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(lanchat_server
  src/app/main.cpp
  src/config/config.cpp
  src/crypto/crypto.cpp
  src/hash/hash.cpp
  src/net/protocol.cpp
  src/net/server.cpp
  src/storage/storage.cpp        # <-- ВАЖНО!
)

target_include_directories(lanchat_server PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/src/util
  ${CMAKE_CURRENT_SOURCE_DIR}/src/config
  ${CMAKE_CURRENT_SOURCE_DIR}/src/net
  ${CMAKE_CURRENT_SOURCE_DIR}/src/storage
  ${CMAKE_CURRENT_SOURCE_DIR}/src/hash
  ${CMAKE_CURRENT_SOURCE_DIR}/src/crypto
)

if (WIN32)
  target_compile_definitions(lanchat_server PRIVATE _WIN32_WINNT=0x0601)
  target_link_libraries(lanchat_server PRIVATE ws2_32 bcrypt)
endif()
