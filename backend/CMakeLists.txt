cmake_minimum_required(VERSION 3.16)
project(reliable_udp_stage1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(rudp_protocol
  src/network/udp_socket.cpp
  src/protocol/serializer.cpp
  src/protocol/parser.cpp
  src/reliability/sender.cpp
  src/reliability/receiver.cpp
  src/reliability/retransmission.cpp
)
target_include_directories(rudp_protocol PUBLIC src)
target_compile_options(rudp_protocol PRIVATE -Wall -Wextra -Wpedantic)

add_executable(udp_client src/client.cpp)
target_link_libraries(udp_client PRIVATE rudp_protocol)

add_executable(udp_server src/server.cpp)
target_link_libraries(udp_server PRIVATE rudp_protocol)
