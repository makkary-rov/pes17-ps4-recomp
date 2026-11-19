// SPDX-License-Identifier: GPL-2.0-or-later
// pes: a sampling profiler for host threads (Windows). PES_PROFILE=<thread name parts, comma-separated>
// ("1" or empty: GpuCommandProcessor) turns it on; every millisecond it records the call stack of each
// matching thread, and every 10 s writes the counted stacks to PES_PROFILE_OUT (default
// pes17-profile.txt in the working directory) as absolute addresses, for llvm-symbolizer.
#pragma once

namespace PesProfiler {
/// Starts the sampler thread if PES_PROFILE is set; otherwise does nothing.
void Start();
}
