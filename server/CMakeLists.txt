cmake_minimum_required(VERSION 3.12)
project(lanchat_server LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(lanchat_server
  src/main.cpp
  src/config.cpp
  src/hash.cpp
  src/protocol.cpp
  src/storage.cpp
  src/server.cpp
  src/db.cpp                    # <— NEW
  third_party/sqlite/sqlite3.c  # <— SQLite amalgamation
)

target_include_directories(lanchat_server PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/sqlite
)

if (WIN32)
  target_compile_definitions(lanchat_server PRIVATE _WIN32_WINNT=0x0601)
  target_link_libraries(lanchat_server PRIVATE ws2_32)
endif()

# Оптимизация для sqlite3.c (уменьшает размер/ускоряет)
if (MSVC)
  target_compile_definitions(lanchat_server PRIVATE SQLITE_THREADSAFE=1 SQLITE_OMIT_LOAD_EXTENSION)
else()
  target_compile_definitions(lanchat_server PRIVATE SQLITE_THREADSAFE=1 SQLITE_OMIT_LOAD_EXTENSION)
endif()

