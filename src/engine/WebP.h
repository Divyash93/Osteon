// Lossless WebP (VP8L) writer, still and animated. Prefix-coded ARGB with a subtract-green transform; no external codec.
#pragma once
#include <cstdint>
#include <vector>

namespace osteon {

/** A complete .webp file for one RGBA image (straight alpha, w*h*4 bytes). Empty on bad input (size 1..16384). */
std::vector<uint8_t> encodeWebpLossless(int w, int h, const uint8_t* rgba);

/** An animated .webp: every frame is w x h RGBA, shown for frameMs milliseconds; loops `loopCount` times (0 = forever). */
std::vector<uint8_t> encodeWebpAnimation(int w, int h, const std::vector<std::vector<uint8_t>>& frames, int frameMs, int loopCount);

} // namespace osteon
