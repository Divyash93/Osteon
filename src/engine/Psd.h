// Minimal Photoshop (PSD/PSB) reader: 8-bit RGB layers with raw or RLE channel data.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace osteon {

struct PsdLayer {
    std::string name;
    int left = 0, top = 0, right = 0, bottom = 0;
    bool visible = true;
    float opacity = 1;
    std::vector<std::string> groupPath; // enclosing groups, outermost first
    std::vector<uint8_t> rgba;          // (right-left)*(bottom-top)*4, straight alpha
    int width() const { return right - left; }
    int height() const { return bottom - top; }
};

struct PsdDocument {
    int width = 0, height = 0;
    std::vector<PsdLayer> layers; // bottom-to-top, as in the file; groups and empty layers are skipped
};

/** Reads pixel layers. Returns false with a readable message for unsupported or damaged files. */
bool readPsd(const std::string& path, PsdDocument& out, std::string& err);
bool readPsdMemory(const std::vector<uint8_t>& data, PsdDocument& out, std::string& err);

} // namespace osteon
