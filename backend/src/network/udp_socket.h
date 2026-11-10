#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace rudp {

int make_udp_socket();
void bind_udp_socket(int socket_fd, std::uint16_t port);
void send_datagram(int socket_fd, const std::string& host, std::uint16_t port,
                   const char* data, std::size_t size);

}  // namespace rudp
