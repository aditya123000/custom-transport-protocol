#include "protocol/serializer.h"

#include <arpa/inet.h>

#include <cstring>
#include <stdexcept>

namespace rudp {

std::vector<char> serialize_packet(const PacketHeader& header, const char* payload,
                                   std::size_t payload_size) {
  if (payload_size != header.payload_size || payload_size > kMaxPayloadSize) {
    throw std::runtime_error("packet payload size is invalid");
  }

  std::vector<char> bytes(kHeaderSize + payload_size);
  const std::uint32_t sequence = htonl(header.seq_num);
  const std::uint32_t acknowledgement = htonl(header.ack_num);
  const std::uint16_t window = htons(header.window_size);
  const std::uint16_t length = htons(header.payload_size);

  std::memcpy(bytes.data(), &sequence, sizeof(sequence));
  std::memcpy(bytes.data() + 4, &acknowledgement, sizeof(acknowledgement));
  std::memcpy(bytes.data() + 8, &window, sizeof(window));
  std::memcpy(bytes.data() + 10, &length, sizeof(length));
  bytes[12] = static_cast<char>(header.flags);
  if (payload_size > 0) {
    std::memcpy(bytes.data() + kHeaderSize, payload, payload_size);
  }
  return bytes;
}

}  // namespace rudp
