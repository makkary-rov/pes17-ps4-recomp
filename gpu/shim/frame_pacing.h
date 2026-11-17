// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <chrono>
#include <cstdint>

namespace PesFramePacing {

using Clock = std::chrono::steady_clock;

class Limiter {
public:
    void Configure(std::chrono::nanoseconds period, Clock::time_point now);
    bool Due(Clock::time_point now) const;
    void Presented(Clock::time_point now);
    Clock::time_point Next() const;

private:
    std::chrono::nanoseconds period_{0};
    Clock::time_point next_{};
};

std::chrono::nanoseconds CurrentPeriod(uint32_t frame_limit);
void SetDisplayRefresh(double hz);
double DisplayRefresh();
void SetVSync(bool on);
bool VSync();

} // namespace PesFramePacing
