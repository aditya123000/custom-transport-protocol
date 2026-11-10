#pragma once

#include "protocol/packet.h"

#include <cstddef>

namespace rudp {

struct ParsedPacket {
  PacketHeader header;
  const char* payload;
};

ParsedPacket parse_packet(const char* bytes, std::size_t received_size);

}  // namespace rudp
