#include "reliability/sender.h"

#include "network/udp_socket.h"
#include "protocol/parser.h"
#include "protocol/serializer.h"

#include <poll.h>
#include <sys/socket.h>

#include <array>
#include <algorithm>
#include <chrono>
#include <iostream>
#include <map>
#include <stdexcept>
#include <utility>

namespace rudp {

ReliableSender::ReliableSender(int socket_fd, std::string host, std::uint16_t port)
    : socket_fd_(socket_fd), host_(std::move(host)), port_(port) {}

void ReliableSender::send_all(const std::vector<PacketToSend>& packets) {
  struct Outstanding { std::vector<char> datagram; std::chrono::steady_clock::time_point sent_at; bool retransmitted; };
  constexpr std::size_t kLocalWindow = 8;
  std::map<std::uint32_t, Outstanding> outstanding;
  std::size_t next = 0;
  while (next < packets.size() || !outstanding.empty()) {
    const auto usable_window = std::min<std::size_t>(kLocalWindow, peer_window_);
    while (next < packets.size() && outstanding.size() < usable_window) {
      const auto& packet = packets[next++];
      auto datagram = serialize_packet(packet.header, packet.payload.data(), packet.payload.size());
      send_datagram(socket_fd_, host_, port_, datagram.data(), datagram.size());
      outstanding.emplace(packet.header.seq_num,
          Outstanding{std::move(datagram), std::chrono::steady_clock::now(), false});
    }
    if (outstanding.empty()) continue;

    const auto deadline = outstanding.begin()->second.sent_at + rto_estimator_.timeout();
    const auto now = std::chrono::steady_clock::now();
    const auto wait = deadline > now ? std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count() : 0;
    pollfd descriptor{socket_fd_, POLLIN, 0};
    const int ready = poll(&descriptor, 1, static_cast<int>(wait));
    if (ready < 0) throw std::runtime_error("poll failed while waiting for ACK");
    if (ready > 0) {
      std::array<char, kHeaderSize + kMaxPayloadSize> received{};
      const auto count = recvfrom(socket_fd_, received.data(), received.size(), 0, nullptr, nullptr);
      if (count < 0) throw std::runtime_error("recvfrom failed while waiting for ACK");
      const auto acknowledgement = parse_packet(received.data(), static_cast<std::size_t>(count));
      if ((acknowledgement.header.flags & ACK) == 0) continue;
      peer_window_ = acknowledgement.header.window_size;
      const auto acknowledged_at = std::chrono::steady_clock::now();
      while (!outstanding.empty() && outstanding.begin()->first < acknowledgement.header.ack_num) {
        const auto& packet = outstanding.begin()->second;
        if (!packet.retransmitted) rto_estimator_.record_rtt(
            std::chrono::duration_cast<std::chrono::milliseconds>(acknowledged_at - packet.sent_at));
        outstanding.erase(outstanding.begin());
      }
      std::cout << "ack=" << acknowledgement.header.ack_num << " advertised_window="
                << peer_window_ << " in_flight=" << outstanding.size() << '\n';
      continue;
    }
    const auto timeout = rto_estimator_.timeout();
    const auto timed_out_at = std::chrono::steady_clock::now();
    bool did_retransmit = false;
    for (auto& [sequence, packet] : outstanding) {
      if (timed_out_at - packet.sent_at >= timeout) {
        send_datagram(socket_fd_, host_, port_, packet.datagram.data(), packet.datagram.size());
        packet.sent_at = timed_out_at;
        packet.retransmitted = true;
        ++retransmissions_;
        did_retransmit = true;
        std::cout << "timeout: seq=" << sequence << " retransmitting\n";
      }
    }
    if (did_retransmit) rto_estimator_.back_off();
  }
}

}  // namespace rudp
