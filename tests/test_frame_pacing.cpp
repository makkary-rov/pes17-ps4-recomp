// SPDX-License-Identifier: GPL-2.0-or-later
#include <cassert>
#include <chrono>

#include "frame_pacing.h"

using namespace std::chrono_literals;
using namespace PesFramePacing;

int main() {
    const auto t0 = Clock::now();

    Limiter limiter;
    limiter.Configure(16ms, t0);
    assert(limiter.Due(t0));
    limiter.Presented(t0);
    assert(limiter.Next() == t0 + 16ms);
    assert(!limiter.Due(t0 + 10ms));
    limiter.Presented(t0 + 17ms);
    assert(limiter.Next() == t0 + 32ms);
    limiter.Presented(t0 + 100ms);
    assert(limiter.Next() == t0 + 116ms);

    limiter.Configure(16ms, t0 + 200ms);
    assert(limiter.Next() == t0 + 116ms);

    limiter.Configure(0ns, t0 + 200ms);
    assert(limiter.Due(t0));
    assert(limiter.Due(t0 + 200ms));

    assert(CurrentPeriod(60) == std::chrono::nanoseconds(16666666));
    assert(CurrentPeriod(0) == std::chrono::nanoseconds(0));

    SetVSync(true);
    assert(VSync());
    SetDisplayRefresh(144.0);
    assert(DisplayRefresh() == 144.0);

    return 0;
}
