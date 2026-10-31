#include "Splash.h"
#include "imgui.h"
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
        float targetW = std::min(640.0f * uiScale, vp->Size.x * 0.55f);
        loaded = tryLoad(gfx, targetW);
        if (!loaded) { failed = true; done = true; release(); return; }
        startTime = ImGui::GetTime();
    }
    float t = (float)(ImGui::GetTime() - startTime);
    if (clock.finished(t)) { done = true; release(); return; }

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
    dl->AddRectFilled(vp->Pos, ImVec2(vp->Pos.x + vp->Size.x, vp->Pos.y + vp->Size.y), IM_COL32(4, 8, 18, (int)(150 * a)));
    ImVec2 cardSize((float)cardW, (float)cardH);
    ImVec2 c0(vp->Pos.x + std::floor((vp->Size.x - cardSize.x) / 2), vp->Pos.y + std::floor((vp->Size.y - cardSize.y) / 2));
    ImVec2 c1(c0.x + cardSize.x, c0.y + cardSize.y);
    ImU32 tint = IM_COL32(255, 255, 255, (int)(255 * a));
    dl->AddImage((ImTextureID)cardTex, c0, c1, ImVec2(0, 0), ImVec2(1, 1), tint);
    int fi = std::min((int)frameTex.size() - 1, (int)(t * pak.fps));
    ImVec2 f0(c0.x + std::floor(pak.placeX * shownScale), c0.y + std::floor(pak.placeY * shownScale));
    dl->AddImage((ImTextureID)frameTex[std::max(0, fi)], f0, ImVec2(f0.x + frameW, f0.y + frameH), ImVec2(0, 0), ImVec2(1, 1), tint);

    // The GITHUB link, and the clicks.
    ImVec2 l0(c0.x + pak.link[0] * shownScale, c0.y + pak.link[1] * shownScale), l1(c0.x + pak.link[2] * shownScale, c0.y + pak.link[3] * shownScale);
    ImVec2 m = ImGui::GetIO().MousePos;
    bool overLink = m.x >= l0.x && m.x <= l1.x && m.y >= l0.y && m.y <= l1.y;
    bool overCard = m.x >= c0.x && m.x <= c1.x && m.y >= c0.y && m.y <= c1.y;
    if (overLink) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        dl->AddLine(ImVec2(l0.x + (l1.x - l0.x) * 0.36f, l1.y - 4 * uiScale), ImVec2(l1.x - 6 * uiScale, l1.y - 4 * uiScale), IM_COL32(255, 255, 255, (int)(200 * a)), 1.5f);
    }
    ImGui::SetCursorScreenPos(vp->Pos);
    ImGui::InvisibleButton("##splashclick", vp->Size);
    if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
        if (overLink && openUrl) openUrl("https://github.com/Divyash93");
        else if (!overCard) clock.dismiss(t); // a click on the empty area around the card closes the splash
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false) || ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_Space, false)) clock.dismiss(t);
    ImGui::End();
}

} // namespace osteon
