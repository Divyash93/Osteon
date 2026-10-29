#include "MathUtil.h"
#include <cstdio>
#include <cstdlib>

namespace osteon {

Color Color::fromHex(const std::string& hexIn, const char* fallback) {
    std::string hex = hexIn.empty() ? std::string(fallback) : hexIn;
    if (!hex.empty() && hex[0] == '#') hex = hex.substr(1);
    if (hex.size() == 6) hex += "ff";
    if (hex.size() != 8) hex = fallback;
    auto n = [&](int i) { return (float)std::strtol(hex.substr(i, 2).c_str(), nullptr, 16) / 255.0f; };
    return {n(0), n(2), n(4), n(6)};
}

std::string Color::toHex(bool alpha) const {
    auto h = [](float v) { return (int)std::lround(clampf(v, 0, 1) * 255); };
    char buf[16];
    if (alpha) std::snprintf(buf, sizeof buf, "%02x%02x%02x%02x", h(r), h(g), h(b), h(a));
    else std::snprintf(buf, sizeof buf, "%02x%02x%02x", h(r), h(g), h(b));
    return buf;
}

uint32_t Color::toABGR() const {
    auto h = [](float v) { return (uint32_t)std::lround(clampf(v, 0, 1) * 255); };
    return h(r) | (h(g) << 8) | (h(b) << 16) | (h(a) << 24);
}

float bezierValue(float time, float t0, float v0, float cx1, float cy1, float cx2, float cy2, float t1, float v1) {
    if (time <= t0) return v0;
    if (time >= t1) return v1;
    auto bx = [&](double s) {
        double u = 1 - s;
        return u * u * u * t0 + 3 * u * u * s * cx1 + 3 * u * s * s * cx2 + s * s * s * t1;
    };
    auto dbx = [&](double s) {
        double u = 1 - s;
        return 3 * u * u * (cx1 - t0) + 6 * u * s * (cx2 - cx1) + 3 * s * s * (t1 - cx2);
    };
    double s = (time - t0) / (t1 - t0);
    for (int i = 0; i < 8; i++) {
        double d = dbx(s);
        if (std::fabs(d) < 1e-9) break;
        double ns = s - (bx(s) - time) / d;
        if (ns < 0 || ns > 1) break;
        s = ns;
    }
    if (std::fabs(bx(s) - time) > 1e-6) {
        double lo = 0, hi = 1;
        for (int i = 0; i < 50; i++) {
            s = (lo + hi) / 2;
            if (bx(s) < time) lo = s; else hi = s;
        }
    }
    double u = 1 - s;
    return (float)(u * u * u * v0 + 3 * u * u * s * cy1 + 3 * u * s * s * cy2 + s * s * s * v1);
}

float distToSegment(Vec2 p, Vec2 a, Vec2 b) {
    Vec2 d = b - a;
    float l2 = d.dot(d);
    float t = l2 > 0 ? clampf((p - a).dot(d) / l2, 0, 1) : 0;
    return (a + d * t - p).len();
}

bool pointInPolygon(float x, float y, const std::vector<float>& poly) {
    bool inside = false;
    size_t n = poly.size() / 2;
    for (size_t i = 0, j = n - 1; i < n; j = i++) {
        float xi = poly[i * 2], yi = poly[i * 2 + 1], xj = poly[j * 2], yj = poly[j * 2 + 1];
        if ((yi > y) != (yj > y) && x < (xj - xi) * (y - yi) / (yj - yi) + xi) inside = !inside;
    }
    return inside;
}

} // namespace osteon
