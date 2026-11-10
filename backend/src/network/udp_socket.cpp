#include "network/udp_socket.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <stdexcept>

namespace rudp {
namespace {

[[noreturn]] void throw_socket_error(const char* operation) {
  throw std::runtime_error(std::string(operation) + ": " + std::strerror(errno));
}

sockaddr_in make_ipv4_address(const std::string& host, std::uint16_t port) {
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_port = htons(port);

  if (inet_pton(AF_INET, host.c_str(), &address.sin_addr) != 1) {
    throw std::runtime_error("host must be an IPv4 address (for example 127.0.0.1)");
  }
  return address;
}

}  // namespace

int make_udp_socket() {
  const int socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (socket_fd < 0) {
    throw_socket_error("socket");
  }
  return socket_fd;
}

void bind_udp_socket(int socket_fd, std::uint16_t port) {
  const sockaddr_in address = make_ipv4_address("0.0.0.0", port);
  if (bind(socket_fd, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) < 0) {
    throw_socket_error("bind");
  }
}

void send_datagram(int socket_fd, const std::string& host, std::uint16_t port,
                   const char* data, std::size_t size) {
  const sockaddr_in destination = make_ipv4_address(host, port);
  const auto sent = sendto(socket_fd, data, size, 0,
                           reinterpret_cast<const sockaddr*>(&destination),
                           sizeof(destination));
  if (sent < 0) {
    throw_socket_error("sendto");
  }
  if (static_cast<std::size_t>(sent) != size) {
    throw std::runtime_error("sendto sent an incomplete UDP datagram");
  }
}

}  // namespace rudp
