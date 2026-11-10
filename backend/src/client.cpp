#include "network/udp_socket.h"
#include "protocol/packet.h"
#include "reliability/sender.h"

#include <unistd.h>

#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
constexpr std::size_t kChunkSize = 1024;

std::uint16_t parse_port(const char* text) {
  const unsigned long value = std::stoul(text);
  if (value == 0 || value > 65535) {
    throw std::runtime_error("port must be between 1 and 65535");
  }
  return static_cast<std::uint16_t>(value);
}
}  // namespace

int main(int argc, char* argv[]) {
  if (argc != 4) {
    std::cerr << "Usage: udp_client <server-ipv4> <port> <input-file>\n";
    return 1;
  }

  try {
    const std::string host = argv[1];
    const std::uint16_t port = parse_port(argv[2]);
    std::ifstream input(argv[3], std::ios::binary);
    if (!input) {
      throw std::runtime_error("could not open input file");
    }

    const int socket_fd = rudp::make_udp_socket();
    rudp::ReliableSender sender(socket_fd, host, port);
    std::array<char, kChunkSize> chunk{};
    std::size_t packets_sent = 0;
    std::uint32_t next_sequence = 1;
    std::vector<rudp::PacketToSend> packets;

    while (input.read(chunk.data(), chunk.size()) || input.gcount() > 0) {
      const std::size_t bytes_read = static_cast<std::size_t>(input.gcount());
      const rudp::PacketHeader header{next_sequence++, 0, 0,
                                      static_cast<std::uint16_t>(bytes_read), rudp::DATA};
      packets.push_back({header, std::vector<char>(chunk.begin(), chunk.begin() + bytes_read)});
      ++packets_sent;
    }
    if (!input.eof()) {
      throw std::runtime_error("error while reading input file");
    }

    const rudp::PacketHeader finish{next_sequence, 0, 0, 0, rudp::FIN};
    packets.push_back({finish, {}});
    sender.send_all(packets);
    close(socket_fd);
    std::cout << "Sent " << packets_sent << " datagrams; retransmissions="
              << sender.retransmissions() << ".\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "udp_client: " << error.what() << '\n';
    return 1;
  }
}
