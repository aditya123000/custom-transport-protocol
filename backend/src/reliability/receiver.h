#pragma once

#include "protocol/packet.h"

#include <map>
#include <vector>

namespace rudp {

struct DeliveredPacket { std::uint8_t flags; std::vector<char> payload; };
struct ReceiveResult {
  std::uint32_t cumulative_ack;
  std::uint16_t advertised_window;
  std::vector<DeliveredPacket> delivered;
};

class ReliableReceiver {
 public:
  ReceiveResult receive(const PacketHeader& header, const char* payload);

 private:
  struct BufferedPacket { std::uint8_t flags; std::vector<char> payload; };
  static constexpr std::uint16_t kBufferCapacity = 8;
  std::uint32_t expected_sequence_ = 1;
  std::map<std::uint32_t, BufferedPacket> reorder_buffer_;
};

}  // namespace rudp
