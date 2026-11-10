#pragma once

#include "protocol/packet.h"

#include <cstddef>
#include <vector>

namespace rudp {

std::vector<char> serialize_packet(const PacketHeader& header, const char* payload,
                                   std::size_t payload_size);

}  // namespace rudp
