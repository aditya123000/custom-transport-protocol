#pragma once

#include "protocol/packet.h"
#include "reliability/retransmission.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace rudp {

struct PacketToSend { PacketHeader header; std::vector<char> payload; };

class ReliableSender {
 public:
  ReliableSender(int socket_fd, std::string host, std::uint16_t port);

  void send_all(const std::vector<PacketToSend>& packets);
  std::size_t retransmissions() const { return retransmissions_; }
  const RtoEstimator& rto_estimator() const { return rto_estimator_; }

 private:
  int socket_fd_;
  std::string host_;
  std::uint16_t port_;
  RtoEstimator rto_estimator_;
  std::size_t retransmissions_ = 0;
  std::uint16_t peer_window_ = 8;
};

}  // namespace rudp
