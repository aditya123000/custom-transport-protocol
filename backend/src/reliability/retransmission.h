#pragma once

#include <chrono>

namespace rudp {

class RtoEstimator {
 public:
  std::chrono::milliseconds timeout() const;
  void record_rtt(std::chrono::milliseconds sample);
  void back_off();

 private:
  static constexpr double kInitialRtoMs = 1000.0;
  double srtt_ms_ = 0.0;
  double rttvar_ms_ = 0.0;
  double rto_ms_ = kInitialRtoMs;
  bool has_sample_ = false;
};

}  // namespace rudp
