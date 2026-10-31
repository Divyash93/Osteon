// Splash screen data: the pack parser, the image resizer and the timing (no graphics, so it is unit tested).
#include "Splash.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace osteon {

// ---- pack parsing ----

SplashPak parseSplashPak(const uint8_t* d, size_t n) {
    SplashPak p;
    const size_t headSize = 4 + 4 * 5 + 4 + 12 + 16; // magic, version, cardW, cardH, frames, reserved, fps, place(3), link(4)
    if (!d || n < headSize || std::memcmp(d, "OSPK", 4) != 0) return p;
    auto u32 = [&](size_t at) { uint32_t v; std::memcpy(&v, d + at, 4); return v; };
    auto f32 = [&](size_t at) { float v; std::memcpy(&v, d + at, 4); return v; };
    if (u32(4) != 1) return p;
    p.cardW = (int)u32(8); p.cardH = (int)u32(12);
    uint32_t frames = u32(16);
    p.fps = f32(24);
    p.placeX = f32(28); p.placeY = f32(32); p.cardPerPixel = f32(36);
    for (int i = 0; i < 4; i++) p.link[i] = f32(40 + 4 * i);
    if (p.cardW <= 0 || p.cardH <= 0 || p.cardW > 8192 || p.cardH > 8192 || frames == 0 || frames > 1000 || !(p.fps > 0) || !(p.cardPerPixel > 0)) return p;
    const size_t entries = (size_t)frames + 1, table = headSize;
    if (table + entries * 8 > n) return p;
    for (size_t i = 0; i < entries; i++) {
        uint32_t off = u32(table + i * 8), size = u32(table + i * 8 + 4);
        if ((size_t)off + size > n || size == 0) return p;
        SplashPak::Blob b{d + off, size};
        if (i == 0) p.card = b; else p.frames.push_back(b);
    }
    p.valid = true;
    return p;
}

// ---- resizing ----

std::vector<uint8_t> resizeRgbaArea(const std::vector<uint8_t>& src, int w, int h, int dw, int dh) {
    if (w <= 0 || h <= 0 || dw <= 0 || dh <= 0 || src.size() < (size_t)w * h * 4) return {};
    std::vector<uint8_t> out((size_t)dw * dh * 4);
    const double sx = (double)w / dw, sy = (double)h / dh;
    for (int y = 0; y < dh; y++) {
        double y0 = y * sy, y1 = (y + 1) * sy;
        int iy0 = (int)y0, iy1 = std::min(h - 1, (int)std::ceil(y1) - 1);
        for (int x = 0; x < dw; x++) {
            double x0 = x * sx, x1 = (x + 1) * sx;
            int ix0 = (int)x0, ix1 = std::min(w - 1, (int)std::ceil(x1) - 1);
            double r = 0, g = 0, b = 0, a = 0, wsum = 0;
            for (int yy = iy0; yy <= iy1; yy++) {
                double wy = std::min(y1, (double)yy + 1) - std::max(y0, (double)yy);
                for (int xx = ix0; xx <= ix1; xx++) {
                    double wx = std::min(x1, (double)xx + 1) - std::max(x0, (double)xx);
                    double wgt = wx * wy;
                    const uint8_t* q = &src[((size_t)yy * w + xx) * 4];
                    double al = q[3] / 255.0;
                    r += q[0] * al * wgt; g += q[1] * al * wgt; b += q[2] * al * wgt; a += al * wgt; wsum += wgt;
                }
            }
            uint8_t* o = &out[((size_t)y * dw + x) * 4];
            if (a > 0) { o[0] = (uint8_t)std::lround(r / a); o[1] = (uint8_t)std::lround(g / a); o[2] = (uint8_t)std::lround(b / a); }
            else o[0] = o[1] = o[2] = 0;
            o[3] = (uint8_t)std::lround(255.0 * (wsum > 0 ? a / wsum : 0));
        }
    }
    return out;
}

// ---- timing ----

float SplashClock::alpha(float t) const {
    float a = fadeIn > 0 ? std::min(1.0f, std::max(0.0f, t / fadeIn)) : 1.0f;
    const float out = std::min(fadeOut, 0.25f);
    float start = -1;                                           // when the fade-out begins
    if (lifetime > 0) start = lifetime - fadeOut;               // a timed splash closes by itself
    if (dismissedAt >= 0) start = start < 0 ? dismissedAt : std::min(start, dismissedAt);
    if (start >= 0 && t > start) a = std::min(a, std::max(0.0f, 1.0f - (t - start) / (dismissedAt >= 0 && dismissedAt <= start + 1e-4f ? out : fadeOut)));
    return a;
}

bool SplashClock::finished(float t) const {
    float end = -1;
    if (lifetime > 0) end = lifetime;
    if (dismissedAt >= 0) { float e = dismissedAt + std::min(fadeOut, 0.25f); end = end < 0 ? e : std::min(end, e); }
    return end >= 0 && t >= end;
}

} // namespace osteon
