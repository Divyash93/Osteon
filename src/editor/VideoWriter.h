// Video containers written by hand (no external encoder): AVI and QuickTime MOV.
#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace osteon {

/** Compresses one RGBA (straight alpha) frame into a PNG or JPEG file image. */
using FrameEncoder = std::function<bool(const uint8_t* rgba, int w, int h, std::vector<uint8_t>& out)>;

/**
 * AVI with one video stream. `encoder` empty = uncompressed 24-bit RGB (no alpha: composite the frames first);
 * otherwise each frame is stored as `encoder` returns it and the stream is tagged `mjpeg` ("MJPG") for a JPEG encoder.
 */
bool writeAvi(const std::string& path, int w, int h, int fps, const std::vector<std::vector<uint8_t>>& frames, const FrameEncoder& encoder);

/**
 * QuickTime MOV with one video track. `codec` is the four-character sample format: "png " (keeps alpha) or "jpeg".
 * Every frame is encoded with `encoder`.
 */
bool writeMov(const std::string& path, int w, int h, int fps, const std::vector<std::vector<uint8_t>>& frames, const FrameEncoder& encoder,
              const char codec[5], bool hasAlpha);

/** The AVI / MOV bytes, for writeAvi / writeMov and for tests. False when the frames or the encoder are unusable. */
bool buildAvi(int w, int h, int fps, const std::vector<std::vector<uint8_t>>& frames, const FrameEncoder& encoder, std::vector<uint8_t>& out);
bool buildMov(int w, int h, int fps, const std::vector<std::vector<uint8_t>>& frames, const FrameEncoder& encoder, const char codec[5],
              bool hasAlpha, std::vector<uint8_t>& out);

} // namespace osteon
