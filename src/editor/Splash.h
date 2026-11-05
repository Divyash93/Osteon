// The start-up splash screen: a card with the animated logo, shown once when Osteon starts.
#pragma once
#include "Gfx.h"
#include <cstdint>
#include <vector>

struct ImVec2;

namespace osteon {

/** The contents of assets/splash/splash.pak (built by tools/make_splash_assets.py), parsed without copying the image data. */
struct SplashPak {
    int cardW = 0, cardH = 0;
    float fps = 30;
    float placeX = 0, placeY = 0, cardPerPixel = 1;   // where the logo frames sit on the card, in card pixels
    float link[4] = {0, 0, 0, 0};                     // the GITHUB link's rectangle on the card (x0, y0, x1, y1)
    struct Blob { const uint8_t* data = nullptr; size_t size = 0; };
    Blob card;
    std::vector<Blob> frames;
    bool valid = false;
};

/** Parses a pack. `valid` is false for anything malformed (wrong magic, offsets outside the buffer, no frames). */
SplashPak parseSplashPak(const uint8_t* data, size_t size);

/** Area-averaging resize of straight-alpha RGBA (alpha-weighted, so soft edges do not darken). */
std::vector<uint8_t> resizeRgbaArea(const std::vector<uint8_t>& src, int w, int h, int dstW, int dstH);

/** Timing of the splash: it fades in and stays until dismissed (or, with a lifetime above 0, closes by itself after that many seconds). */
struct SplashClock {
    float lifetime = 0, fadeIn = 0.25f, fadeOut = 0.35f; // lifetime 0: stays until it is dismissed
    float dismissedAt = -1;                    // time of a click / key, or -1
    /** 0..1 opacity at time t (seconds since the splash began). */
    float alpha(float t) const;
    /** True once the splash has completely faded out. */
    bool finished(float t) const;
    void dismiss(float t) { if (dismissedAt < 0) dismissedAt = t; }
};

class SplashScreen {
public:
    bool enabled = true;
    bool fillWindow = false; // the window is the size of the card: the card fills it
    bool active() const { return enabled && !done; }
    /** Draws the splash over the whole main window (call at the end of a frame). Handles its own clicks. */
    void draw(Gfx& gfx, float uiScale, void (*openUrl)(const char*));
    void release();
    void skip() { done = true; }

private:
    bool tryLoad(Gfx& gfx, float targetWidth);
    bool loaded = false, failed = false, done = false;
    double startTime = -1;
    SplashClock clock;
    SplashPak pak;
    void* cardTex = nullptr;
    std::vector<void*> frameTex;
    int cardW = 0, cardH = 0, frameW = 0, frameH = 0;   // as uploaded (already resized for the screen)
    float shownScale = 1;                                // uploaded pixels per card pixel
};

} // namespace osteon
