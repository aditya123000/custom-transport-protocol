#include "reliability/receiver.h"

namespace rudp {

ReceiveResult ReliableReceiver::receive(const PacketHeader& header, const char* payload) {
  std::vector<DeliveredPacket> delivered;
  if (header.seq_num == expected_sequence_) {
    delivered.push_back({header.flags, std::vector<char>(payload, payload + header.payload_size)});
    ++expected_sequence_;
    auto buffered = reorder_buffer_.find(expected_sequence_);
    while (buffered != reorder_buffer_.end()) {
      delivered.push_back({buffered->second.flags, std::move(buffered->second.payload)});
      reorder_buffer_.erase(buffered);
      ++expected_sequence_;
      buffered = reorder_buffer_.find(expected_sequence_);
    }
  } else if (header.seq_num > expected_sequence_ &&
             header.seq_num < expected_sequence_ + kBufferCapacity &&
             reorder_buffer_.size() < kBufferCapacity &&
             reorder_buffer_.find(header.seq_num) == reorder_buffer_.end()) {
    reorder_buffer_.emplace(header.seq_num,
        BufferedPacket{header.flags, std::vector<char>(payload, payload + header.payload_size)});
  }
  return {expected_sequence_, static_cast<std::uint16_t>(kBufferCapacity - reorder_buffer_.size()),
          std::move(delivered)};
}

}  // namespace rudp
