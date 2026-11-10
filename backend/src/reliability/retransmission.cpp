#include "reliability/retransmission.h"

#include <algorithm>
#include <cmath>

namespace rudp {
namespace {
constexpr double kMinRtoMs = 100.0;
constexpr double kMaxRtoMs = 60000.0;
}

std::chrono::milliseconds RtoEstimator::timeout() const {
  return std::chrono::milliseconds(static_cast<long long>(rto_ms_));
}

void RtoEstimator::record_rtt(std::chrono::milliseconds sample) {
  const double sample_ms = static_cast<double>(sample.count());
  if (!has_sample_) {
    srtt_ms_ = sample_ms;
    rttvar_ms_ = sample_ms / 2.0;
    has_sample_ = true;
  } else {
    // RFC 6298 / Jacobson: update variation using the previous SRTT first.
    rttvar_ms_ = 0.75 * rttvar_ms_ + 0.25 * std::abs(srtt_ms_ - sample_ms);
    srtt_ms_ = 0.875 * srtt_ms_ + 0.125 * sample_ms;
  }
  rto_ms_ = std::clamp(srtt_ms_ + 4.0 * rttvar_ms_, kMinRtoMs, kMaxRtoMs);
}

void RtoEstimator::back_off() {
  rto_ms_ = std::min(rto_ms_ * 2.0, kMaxRtoMs);
}

}  // namespace rudp
