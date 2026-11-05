#include "Splash.h"
#include "imgui.h"
#include "I18n.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <windows.h>

namespace osteon {

// ---- the screen ----

static std::vector<uint8_t> resourceBytes(int id) {
    HRSRC r = FindResourceW(nullptr, MAKEINTRESOURCEW(id), RT_RCDATA);
    if (!r) return {};
    HGLOBAL g = LoadResource(nullptr, r);
    DWORD size = SizeofResource(nullptr, r);
    const uint8_t* p = g ? (const uint8_t*)LockResource(g) : nullptr;
    return p ? std::vector<uint8_t>(p, p + size) : std::vector<uint8_t>();
}

bool SplashScreen::tryLoad(Gfx& gfx, float targetWidth) {
    static std::vector<uint8_t> bytes = resourceBytes(301); // the pack stays alive: the parsed blobs point into it
    pak = parseSplashPak(bytes.data(), bytes.size());
    if (!pak.valid) return false;
    shownScale = std::max(0.2f, targetWidth / (float)pak.cardW);
    std::vector<uint8_t> px;
    int w = 0, h = 0;
    if (!decodeImageMemory(pak.card.data, pak.card.size, w, h, px)) return false;
    cardW = std::max(1, (int)std::lround(w * shownScale)); cardH = std::max(1, (int)std::lround(h * shownScale));
    cardTex = gfx.createTexture(cardW, cardH, resizeRgbaArea(px, w, h, cardW, cardH).data());
    if (!cardTex) return false;
    frameTex.clear();
    for (auto& b : pak.frames) {
        std::vector<uint8_t> fp;
        int fw = 0, fh = 0;
        if (!decodeImageMemory(b.data, b.size, fw, fh, fp)) return false;
        // A stored frame pixel covers cardPerPixel card pixels, i.e. cardPerPixel * shownScale uploaded pixels.
        frameW = std::max(1, (int)std::lround(fw * pak.cardPerPixel * shownScale));
        frameH = std::max(1, (int)std::lround(fh * pak.cardPerPixel * shownScale));
        frameTex.push_back(gfx.createTexture(frameW, frameH, resizeRgbaArea(fp, fw, fh, frameW, frameH).data()));
        if (!frameTex.back()) return false;
    }
    return true;
}

void SplashScreen::release() {
    if (cardTex) { ((ID3D11ShaderResourceView*)cardTex)->Release(); cardTex = nullptr; }
    for (void* t : frameTex) if (t) ((ID3D11ShaderResourceView*)t)->Release();
    frameTex.clear();
}

void SplashScreen::draw(Gfx& gfx, float uiScale, void (*openUrl)(const char*)) {
    if (!active()) return;
    ImGuiViewport* vp = ImGui::GetMainViewport();
    if (!loaded && !failed) {
        // The card is shown about 640 logical pixels wide, but never more than 55% of the window.
        float targetW = fillWindow ? vp->Size.x : std::min(640.0f * uiScale, vp->Size.x * 0.55f);
        loaded = tryLoad(gfx, targetW);
        if (!loaded) { failed = true; done = true; release(); return; }
        startTime = ImGui::GetTime();
        clock.lifetime = 2.9f; // the splash hands over to the Home window by itself
    }
    float t = (float)(ImGui::GetTime() - startTime);
    if (clock.finished(t)) {
        done = true; release();
        ImGui::GetForegroundDrawList()->AddRectFilled(vp->Pos, ImVec2(vp->Pos.x + vp->Size.x, vp->Pos.y + vp->Size.y), IM_COL32(5, 9, 20, 255)); // no flash of the editor on the last frame
        return;
    }

    // A window over everything captures the mouse, so nothing underneath reacts while the splash is up.
    ImGui::SetNextWindowPos(vp->Pos);
    ImGui::SetNextWindowSize(vp->Size);
    ImGui::SetNextWindowFocus();
    ImGuiWindowFlags fl = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking |
                          ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollWithMouse;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
    ImGui::Begin("##splash", nullptr, fl);
    ImGui::PopStyleVar(2);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float a = clock.alpha(t);
    // The whole window is covered and fully opaque from the first frame: the editor is not shown until the Home window has had its turn.
    dl->AddRectFilledMultiColor(vp->Pos, ImVec2(vp->Pos.x + vp->Size.x, vp->Pos.y + vp->Size.y), IM_COL32(5, 9, 20, 255), IM_COL32(10, 14, 32, 255), IM_COL32(8, 8, 22, 255), IM_COL32(3, 6, 14, 255));
    ImVec2 cardSize((float)cardW, (float)cardH);
    ImVec2 c0(vp->Pos.x + std::floor((vp->Size.x - cardSize.x) / 2), vp->Pos.y + std::floor((vp->Size.y - cardSize.y) / 2));
    ImVec2 c1(c0.x + cardSize.x, c0.y + cardSize.y);
    if (fillWindow) { c0 = vp->Pos; c1 = ImVec2(vp->Pos.x + vp->Size.x, vp->Pos.y + vp->Size.y); }
    ImU32 tint = IM_COL32(255, 255, 255, (int)(255 * a));
    dl->AddImage((ImTextureID)cardTex, c0, c1, ImVec2(0, 0), ImVec2(1, 1), tint);
    int fi = std::min((int)frameTex.size() - 1, (int)(t * pak.fps));
    ImVec2 f0(c0.x + std::floor(pak.placeX * shownScale), c0.y + std::floor(pak.placeY * shownScale));
    dl->AddImage((ImTextureID)frameTex[std::max(0, fi)], f0, ImVec2(f0.x + frameW, f0.y + frameH), ImVec2(0, 0), ImVec2(1, 1), tint);

    // Title block and loader, drawn live so they follow the language. Sizes are logical pixels times the UI scale.
    const float u = uiScale;
    const float margin = std::floor(pak.placeX * 0 + 105 * shownScale); // aligned with the logo's left edge
    ImFont* font = ImGui::GetFont();
    auto text = [&](float size, float x, float y, ImU32 col, const char* s) { dl->AddText(font, size, ImVec2(x, y), col, s); };
    auto alphaCol = [&](int r, int g, int b, float k) { return IM_COL32(r, g, b, (int)(255 * std::min(1.0f, k * a))); };
    const float top = c1.y - 190 * u;
    float rise = 1.0f - std::min(1.0f, t / 0.9f);           // the text eases up into place
    float lift = rise * rise * 10 * u;
    text(11.5f * u, c0.x + margin, top + lift, alphaCol(120, 215, 255, 0.95f), (std::string(tr("VERSION")) + "  0.1.0").c_str());
    text(27 * u, c0.x + margin, top + 20 * u + lift, alphaCol(255, 255, 255, 1.0f), tr("2D skeletal animation, made simple."));
    text(14 * u, c0.x + margin, top + 58 * u + lift, alphaCol(190, 200, 225, 0.9f), tr("Rig, animate and export for Unity and Godot."));

    // The loader: a thin bar with a glowing head, and what the program is doing.
    float life = clock.lifetime > 0 ? clock.lifetime - clock.fadeOut : 2.5f;
    float p = std::min(1.0f, t / life);
    p = p * p * (3 - 2 * p);                                  // smoothstep
    float bx0 = c0.x + margin, bx1 = c1.x - margin, by = c1.y - 44 * u, bh = std::max(2.0f, 2.5f * u);
    const char* stage = p < 0.35f ? tr("Starting up") : p < 0.8f ? tr("Loading interface") : tr("Ready");
    text(12 * u, bx0, by - 22 * u, alphaCol(190, 200, 225, 0.85f), stage);
    dl->AddRectFilled(ImVec2(bx0, by), ImVec2(bx1, by + bh), alphaCol(255, 255, 255, 0.14f), bh);
    float fx = bx0 + (bx1 - bx0) * p;
    if (fx > bx0 + 1) {
        dl->AddRectFilledMultiColor(ImVec2(bx0, by), ImVec2(fx, by + bh), alphaCol(60, 200, 255, 1), alphaCol(150, 110, 255, 1), alphaCol(150, 110, 255, 1), alphaCol(60, 200, 255, 1));
        dl->AddCircleFilled(ImVec2(fx, by + bh / 2), 6 * u, alphaCol(150, 110, 255, 0.28f));
        dl->AddCircleFilled(ImVec2(fx, by + bh / 2), 3 * u, alphaCol(255, 255, 255, 1.0f));
    }

    // Any click or key hands over to the Home window straight away.
    ImGui::SetCursorScreenPos(vp->Pos);
    ImGui::InvisibleButton("##splashclick", vp->Size);
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) clock.dismiss(t);
    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false) || ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_Space, false)) clock.dismiss(t);
    ImGui::End();
}

} // namespace osteon
