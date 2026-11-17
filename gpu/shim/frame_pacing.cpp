// SPDX-License-Identifier: GPL-2.0-or-later
#include "frame_pacing.h"

#include <atomic>

namespace PesFramePacing {

namespace {
std::atomic<double> display_refresh{60.0};
std::atomic<bool> vsync{false};
} // namespace

void Limiter::Configure(std::chrono::nanoseconds period, Clock::time_point now) {
    // Called on every paused loop: keep the phase unless the period changes.
    if (period != period_) {
        period_ = period;
        next_ = now;
    }
}

bool Limiter::Due(Clock::time_point now) const {
    return period_.count() <= 0 || now >= next_;
}

void Limiter::Presented(Clock::time_point now) {
    if (period_.count() <= 0) {
        next_ = now;
        return;
    }
    next_ += period_;
    if (next_ < now) {
        next_ = now + period_;
    }
}

Clock::time_point Limiter::Next() const {
    return next_;
}

std::chrono::nanoseconds CurrentPeriod(uint32_t frame_limit) {
    return frame_limit ? std::chrono::nanoseconds(1000000000 / frame_limit)
                       : std::chrono::nanoseconds(0);
}

void SetDisplayRefresh(double hz) {
    display_refresh.store(hz, std::memory_order_relaxed);
}

double DisplayRefresh() {
    return display_refresh.load(std::memory_order_relaxed);
}

void SetVSync(bool on) {
    vsync.store(on, std::memory_order_relaxed);
}

bool VSync() {
    return vsync.load(std::memory_order_relaxed);
}

} // namespace PesFramePacing
