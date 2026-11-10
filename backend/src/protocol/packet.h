#pragma once

#include <cstddef>
#include <cstdint>

namespace rudp {

struct PacketHeader {
  std::uint32_t seq_num;
  std::uint32_t ack_num;
  std::uint16_t window_size;
  std::uint16_t payload_size;
  std::uint8_t flags;
};

constexpr std::uint8_t SYN = 1U << 0;
constexpr std::uint8_t ACK = 1U << 1;
constexpr std::uint8_t FIN = 1U << 2;
constexpr std::uint8_t DATA = 1U << 3;

// This is a wire-format size, independent of compiler struct padding.
constexpr std::size_t kHeaderSize = 13;
constexpr std::size_t kMaxPayloadSize = 1024;

}  // namespace rudp
