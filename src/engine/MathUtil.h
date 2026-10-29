// Math helpers shared by the engine. Public angles are degrees; y is up.
#pragma once
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>
#include <algorithm>

namespace osteon {

constexpr float PI = 3.14159265358979323846f;
constexpr float PI2 = PI * 2;
constexpr float degRad = PI / 180.0f;
constexpr float radDeg = 180.0f / PI;

inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
inline float cosDeg(float d) { return std::cos(d * degRad); }
inline float sinDeg(float d) { return std::sin(d * degRad); }
inline float atan2Deg(float y, float x) { return std::atan2(y, x) * radDeg; }

/** Wraps degrees into (-180, 180]. */
inline float wrapDeg(float d) {
    d = std::fmod(d, 360.0f);
    if (d > 180) d -= 360;
    else if (d <= -180) d += 360;
    return d;
}

inline float wrapRad(float r) {
    if (r > PI) return r - PI2;
    if (r < -PI) return r + PI2;
    return r;
}

struct Vec2 {
    float x = 0, y = 0;
    Vec2() = default;
    Vec2(float x_, float y_) : x(x_), y(y_) {}
    Vec2 operator+(Vec2 o) const { return {x + o.x, y + o.y}; }
    Vec2 operator-(Vec2 o) const { return {x - o.x, y - o.y}; }
    Vec2 operator*(float s) const { return {x * s, y * s}; }
    float len() const { return std::sqrt(x * x + y * y); }
    float dot(Vec2 o) const { return x * o.x + y * o.y; }
};

struct Color {
    float r = 1, g = 1, b = 1, a = 1;
    Color() = default;
    Color(float r_, float g_, float b_, float a_ = 1) : r(r_), g(g_), b(b_), a(a_) {}
    bool operator==(const Color& o) const { return r == o.r && g == o.g && b == o.b && a == o.a; }
    bool operator!=(const Color& o) const { return !(*this == o); }
    Color operator*(const Color& o) const { return {r * o.r, g * o.g, b * o.b, a * o.a}; }
    static Color fromHex(const std::string& hexIn, const char* fallback = "ffffffff");
    std::string toHex(bool alpha = true) const;
    uint32_t toABGR() const;
};

/**
 * Cubic Bezier animation-curve segment with absolute control points (time, value),
 * matching the 4.x JSON curve format. Returns the value at `time`.
 */
float bezierValue(float time, float t0, float v0, float cx1, float cy1, float cx2, float cy2, float t1, float v1);

float distToSegment(Vec2 p, Vec2 a, Vec2 b);
bool pointInPolygon(float x, float y, const std::vector<float>& poly);

} // namespace osteon
