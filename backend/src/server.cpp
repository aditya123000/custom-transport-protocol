#include "network/udp_socket.h"
#include "protocol/packet.h"
#include "protocol/parser.h"
#include "protocol/serializer.h"
#include "reliability/receiver.h"

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
std::uint16_t parse_port(const char* text) {
  const unsigned long value = std::stoul(text);
  if (value == 0 || value > 65535) {
    throw std::runtime_error("port must be between 1 and 65535");
  }
  return static_cast<std::uint16_t>(value);
}
}  // namespace

int main(int argc, char* argv[]) {
  if (argc != 3) {
    std::cerr << "Usage: udp_server <port> <output-file>\n";
    return 1;
  }

  try {
    const int socket_fd = rudp::make_udp_socket();
    rudp::bind_udp_socket(socket_fd, parse_port(argv[1]));

    std::ofstream output(argv[2], std::ios::binary | std::ios::trunc);
    if (!output) {
      throw std::runtime_error("could not open output file");
    }

    std::array<char, rudp::kHeaderSize + rudp::kMaxPayloadSize> datagram{};
    std::size_t packets_received = 0;
    rudp::ReliableReceiver receiver;
    for (;;) {
      sockaddr_in peer{};
      socklen_t peer_length = sizeof(peer);
      const auto received = recvfrom(socket_fd, datagram.data(), datagram.size(), 0,
                                     reinterpret_cast<sockaddr*>(&peer), &peer_length);
      if (received < 0) {
        throw std::runtime_error("recvfrom failed");
      }
      const auto packet = rudp::parse_packet(datagram.data(),
                                             static_cast<std::size_t>(received));
      std::cout << "header: seq=" << packet.header.seq_num
                << " ack=" << packet.header.ack_num
                << " window=" << packet.header.window_size
                << " payload=" << packet.header.payload_size
                << " flags=" << static_cast<unsigned>(packet.header.flags) << '\n';
      if ((packet.header.flags & (rudp::DATA | rudp::FIN)) == 0) {
        throw std::runtime_error("received a packet without DATA or FIN flag");
      }
      const auto result = receiver.receive(packet.header, packet.payload);
      const rudp::PacketHeader ack{0, result.cumulative_ack, result.advertised_window, 0, rudp::ACK};
      const auto ack_datagram = rudp::serialize_packet(ack, nullptr, 0);
      const auto ack_sent = sendto(socket_fd, ack_datagram.data(), ack_datagram.size(), 0,
                                   reinterpret_cast<const sockaddr*>(&peer), peer_length);
      if (ack_sent != static_cast<ssize_t>(ack_datagram.size())) {
        throw std::runtime_error("failed to send ACK");
      }
      if (result.delivered.empty()) {
        std::cout << "discarded: seq=" << packet.header.seq_num
                  << " cumulative_ack=" << result.cumulative_ack << '\n';
        continue;
      }
      bool finished = false;
      for (const auto& delivered : result.delivered) {
        if ((delivered.flags & rudp::FIN) != 0) {
          if (!delivered.payload.empty()) throw std::runtime_error("FIN packet must not contain a payload");
          finished = true;
          continue;
        }
        output.write(delivered.payload.data(), delivered.payload.size());
        if (!output) throw std::runtime_error("failed to write output file");
        ++packets_received;
      }
      if (finished) break;
    }

    close(socket_fd);
    std::cout << "Received " << packets_received << " datagrams.\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "udp_server: " << error.what() << '\n';
    return 1;
  }
}
