#include "protocol/parser.h"

#include <arpa/inet.h>

#include <cstring>
#include <stdexcept>

namespace rudp {

ParsedPacket parse_packet(const char* bytes, std::size_t received_size) {
  if (received_size < kHeaderSize) {
    throw std::runtime_error("datagram is smaller than the protocol header");
  }

  std::uint32_t sequence{};
  std::uint32_t acknowledgement{};
  std::uint16_t window{};
  std::uint16_t length{};
  std::memcpy(&sequence, bytes, sizeof(sequence));
  std::memcpy(&acknowledgement, bytes + 4, sizeof(acknowledgement));
  std::memcpy(&window, bytes + 8, sizeof(window));
  std::memcpy(&length, bytes + 10, sizeof(length));

  PacketHeader header{ntohl(sequence), ntohl(acknowledgement), ntohs(window),
                      ntohs(length), static_cast<std::uint8_t>(bytes[12])};
  if (header.payload_size > kMaxPayloadSize ||
      header.payload_size != received_size - kHeaderSize) {
    throw std::runtime_error("datagram payload size does not match its header");
  }
  return {header, bytes + kHeaderSize};
}

}  // namespace rudp
