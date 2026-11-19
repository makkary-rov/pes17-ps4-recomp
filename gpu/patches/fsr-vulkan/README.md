<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# Local changes to gpu/third_party/fsr-vulkan

The submodule points at upstream FireBurn/FSR-Vulkan (`c64f093`). `build.sh` applies the
patches here to its working tree when they are not applied yet:

- `0001-...`: `PES_FSR4_PROFILE` (GPU time per FSR 4 pass) and `PES_FSR4_STATS` (driver
  statistics of each pass) in the FSR 4 v07 provider.
