// The Home window: shown after the splash at start-up (and from Help > Home). Recent projects, New / Open / Import, and links to the guide, the source and the docs.
#include "App.h"
#include "I18n.h"
#include "Icons.h"
#include "Theme.h"
#include "Util.h"
#include "imgui.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>

namespace osteon {

namespace {

const char* kLearnUrl = "https://divyash93.github.io/Osteon2D/getting-started.html";
const char* kGithubUrl = "https://github.com/Divyash93/Osteon2D";
const char* kDocsUrl = "https://divyash93.github.io/Osteon2D/";

ImU32 withAlpha(ImU32 c, float k) {
    unsigned a = (unsigned)(((c >> 24) & 0xFF) * std::max(0.0f, std::min(1.0f, k)));
    return (c & 0x00FFFFFF) | (a << 24);
}

// A short stable colour pair for a project, so tiles are told apart without thumbnails.
void tileColors(const std::string& s, ImU32& top, ImU32& bottom) {
    unsigned h = 2166136261u;
    for (unsigned char c : s) h = (h ^ c) * 16777619u;
    float hue = (h % 360) / 360.0f;
    float r, g, b;
    ImGui::ColorConvertHSVtoRGB(hue, 0.42f, 0.46f, r, g, b);
    top = ImGui::ColorConvertFloat4ToU32(ImVec4(r, g, b, 1));
    ImGui::ColorConvertHSVtoRGB(std::fmod(hue + 0.08f, 1.0f), 0.55f, 0.22f, r, g, b);
    bottom = ImGui::ColorConvertFloat4ToU32(ImVec4(r, g, b, 1));
}

std::string baseName(const std::string& path) {
    std::string n = fileName(path);
    size_t dot = n.find_last_of('.');
    return dot == std::string::npos || dot == 0 ? n : n.substr(0, dot);
}

std::string ago(long long seconds) {
    if (seconds < 0) seconds = 0;
    long long days = seconds / 86400;
    if (days < 1) return tr("Today");
    if (days == 1) return tr("Yesterday");
    if (days < 14) return std::to_string(days) + " " + tr("days ago");
    if (days < 60) return std::to_string(days / 7) + " " + tr("weeks ago");
    if (days < 730) return std::to_string(days / 30) + " " + tr("months ago");
    return std::to_string(days / 365) + " " + tr("years ago");
}

} // namespace

void Editor::refreshHome() {
    homeItems.clear();
    auto nowSys = std::chrono::system_clock::now();
    for (auto& p : doc.recentFiles) {
        HomeItem it;
        it.path = p;
        it.name = baseName(p);
        it.ext = extension(p);
        it.exists = fileExists(p);
        it.age = -1;
        if (it.exists) {
            std::error_code ec;
            auto ft = std::filesystem::last_write_time(std::filesystem::path(widen(p)), ec);
            if (!ec) {
                auto sys = std::chrono::clock_cast<std::chrono::system_clock>(ft);
                it.age = (long long)std::chrono::duration_cast<std::chrono::seconds>(nowSys - sys).count();
            }
        }
        homeItems.push_back(std::move(it));
    }
    homeDirty = false;
}

void Editor::openHome() {
    showHome = true;
    homeDirty = true;
    homeOpenedAt = -1;
}

void Editor::homeWindow() {
    if (!showHome || splash.active()) return;
    if (recovery.pending) { showHome = false; return; }
    if (homeDirty) refreshHome();
    if (homeOpenedAt < 0) homeOpenedAt = ImGui::GetTime();

    const float u = uiScale;
    ImGuiViewport* vp = ImGui::GetMainViewport();
    const ImVec2 pos = vp->WorkPos, size = vp->WorkSize;
    const float fade = 1.0f; // opaque at once, so the editor never flashes through between the splash and Home

    ImGui::SetNextWindowPos(pos);
    ImGui::SetNextWindowSize(size);
    ImGui::SetNextWindowFocus();
    ImGuiWindowFlags fl = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDocking |
                          ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoScrollWithMouse;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, fade);
    ImGui::Begin("##home", nullptr, fl);
    ImGui::PopStyleVar(2);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImFont* font = ImGui::GetFont();
    const ImU32 colText = uiTextColor(), colDim = uiDimTextColor();
    const ImU32 colBg = ImGui::GetColorU32(ImGuiCol_WindowBg);
    const ImU32 colRaised = ImGui::GetColorU32(uiRaised());
    const ImU32 colRaisedHi = ImGui::GetColorU32(uiRaisedHi());
    const ImU32 colBorder = ImGui::GetColorU32(ImGuiCol_Border);
    const ImU32 colAccent = theme::hex(theme::kSelectedBg), colAccentHi = theme::hex(theme::kSelectedHi);
    const ImU32 colTeal = IM_COL32(135, 209, 219, 255);
    auto text = [&](float sz, ImVec2 p, ImU32 c, const char* s, float clipW = 0) {
        if (clipW > 0) { ImVec4 clip(p.x, p.y - 2, p.x + clipW, p.y + sz + 4); dl->AddText(font, sz, p, c, s, nullptr, 0, &clip); }
        else dl->AddText(font, sz, p, c, s);
    };
    auto textW = [&](float sz, const char* s) { return font->CalcTextSizeA(sz, 100000, 0, s).x; };
    auto hit = [&](const char* id, ImVec2 p, ImVec2 sz, bool* hovered = nullptr) {
        ImGui::SetCursorScreenPos(p);
        bool c = ImGui::InvisibleButton(id, sz);
        if (hovered) *hovered = ImGui::IsItemHovered();
        return c;
    };

    const ImU32 colMain = gLightUi ? colBg : theme::hex(theme::kAppBg); // the content area sits a step darker than the sidebar and the tiles
    dl->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), colMain);
    // ---- sidebar ----
    const float sideW = std::floor(std::min(268 * u, size.x * 0.3f));
    dl->AddRectFilled(pos, ImVec2(pos.x + sideW, pos.y + size.y), colRaised);
    dl->AddLine(ImVec2(pos.x + sideW, pos.y), ImVec2(pos.x + sideW, pos.y + size.y), colBorder);
    const float sx = pos.x + 22 * u, sw = sideW - 44 * u;
    float y = pos.y + 22 * u;
    if (headerLogo) dl->AddImage((ImTextureID)headerLogo, ImVec2(sx, y), ImVec2(sx + 26 * u, y + 26 * u));
    text(19 * u, ImVec2(sx + 36 * u, y + 2 * u), colText, "Osteon");
    y += 58 * u;

    {   // New project: the one filled button
        bool hv;
        bool clicked = hit("##homenew", ImVec2(sx, y), ImVec2(sw, 38 * u), &hv);
        dl->AddRectFilled(ImVec2(sx, y), ImVec2(sx + sw, y + 38 * u), hv ? colAccentHi : colAccent, 19 * u);
        drawIcon(dl, Icon::DocNew, ImVec2(sx + 22 * u, y + 19 * u), 15 * u, IM_COL32(255, 255, 255, 255));
        text(14 * u, ImVec2(sx + 40 * u, y + 11 * u), IM_COL32(255, 255, 255, 255), tr("New project"));
        if (clicked) { showHome = false; cmdNew(); }
        y += 52 * u;
    }
    auto sideRow = [&](const char* id, Icon icon, const char* label, bool selected) {
        bool hv;
        bool c = hit(id, ImVec2(sx - 8 * u, y), ImVec2(sw + 16 * u, 34 * u), &hv);
        if (selected || hv) dl->AddRectFilled(ImVec2(sx - 8 * u, y), ImVec2(sx + sw + 8 * u, y + 34 * u), selected ? colRaisedHi : withAlpha(colRaisedHi, 0.7f), 6 * u);
        if (selected) dl->AddRectFilled(ImVec2(sx - 8 * u, y + 7 * u), ImVec2(sx - 5 * u, y + 27 * u), colTeal, 2 * u);
        drawIcon(dl, icon, ImVec2(sx + 10 * u, y + 17 * u), 15 * u, selected ? colTeal : colDim);
        text(14 * u, ImVec2(sx + 30 * u, y + 8 * u), selected ? colText : colDim, label);
        y += 38 * u;
        return c;
    };
    std::vector<std::string> before = {doc.projectPath, std::to_string(doc.data.bones.size()), std::to_string(doc.data.slots.size())};
    auto changed = [&] { return before != std::vector<std::string>{doc.projectPath, std::to_string(doc.data.bones.size()), std::to_string(doc.data.slots.size())}; };
    if (sideRow("##homeopen", Icon::Folder, tr("Open project..."), false)) { showHome = false; cmdOpen(); if (!changed()) openHome(); }
    if (sideRow("##homepsd", Icon::Psd, tr("Import PSD..."), false)) { showHome = false; cmdImportPsd(); if (!changed()) openHome(); }
    y += 6 * u;
    dl->AddLine(ImVec2(sx, y), ImVec2(sx + sw, y), colBorder);
    y += 10 * u;
    sideRow("##homehome", Icon::Skeleton, tr("Home"), true);

    {   // The three resource cards, anchored at the bottom of the sidebar.
        struct Res { const char* id; Icon icon; const char* title; const char* sub; const char* url; ImU32 tint; };
        const Res res[3] = {
            {"##resLearn", Icon::Question, tr("Learn Osteon"), tr("Step-by-step guide"), kLearnUrl, IM_COL32(86, 190, 220, 255)},
            {"##resGit", Icon::Globe, tr("GitHub"), tr("Source and releases"), kGithubUrl, IM_COL32(150, 120, 240, 255)},
            {"##resDocs", Icon::Atlas, tr("Docs"), tr("Runtimes, formats, languages"), kDocsUrl, IM_COL32(110, 200, 140, 255)}};
        const float ch = 58 * u, gap = 8 * u;
        float cy = pos.y + size.y - 22 * u - (3 * ch + 2 * gap);
        text(11 * u, ImVec2(sx, cy - 20 * u), colDim, tr("RESOURCES"));
        for (const Res& r : res) {
            bool hv;
            bool c = hit(r.id, ImVec2(sx, cy), ImVec2(sw, ch), &hv);
            dl->AddRectFilled(ImVec2(sx, cy), ImVec2(sx + sw, cy + ch), hv ? colRaisedHi : colMain, 9 * u);
            dl->AddRect(ImVec2(sx, cy), ImVec2(sx + sw, cy + ch), hv ? withAlpha(r.tint, 0.9f) : colBorder, 9 * u);
            dl->AddRectFilled(ImVec2(sx + 10 * u, cy + 11 * u), ImVec2(sx + 46 * u, cy + 47 * u), withAlpha(r.tint, 0.2f), 8 * u);
            drawIcon(dl, r.icon, ImVec2(sx + 28 * u, cy + 29 * u), 17 * u, r.tint);
            text(14 * u, ImVec2(sx + 58 * u, cy + 12 * u), colText, r.title);
            text(11 * u, ImVec2(sx + 58 * u, cy + 33 * u), colDim, r.sub, sw - 66 * u);
            if (hv) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            if (c) openUrl(r.url);
            cy += ch + gap;
        }
    }

    // ---- main area ----
    const float mx = pos.x + sideW + 44 * u, mr = pos.x + size.x - 44 * u;
    float my = pos.y + 40 * u;
    text(30 * u, ImVec2(mx, my), colText, tr("Welcome to Osteon"));
    text(14 * u, ImVec2(mx, my + 42 * u), colDim, tr("2D skeletal animation, made simple."));
    dl->AddRectFilled(ImVec2(mx, my + 36 * u), ImVec2(mx + 44 * u, my + 38 * u), colTeal, 1);   // a small accent rule under the title

    {   // close
        bool hv;
        ImVec2 c0(mr - 30 * u, pos.y + 18 * u);
        bool c = hit("##homeclose", c0, ImVec2(30 * u, 30 * u), &hv);
        if (hv) dl->AddRectFilled(c0, ImVec2(c0.x + 30 * u, c0.y + 30 * u), colRaisedHi, 6 * u);
        ImVec2 m(c0.x + 15 * u, c0.y + 15 * u);
        float k = 5 * u;
        dl->AddLine(ImVec2(m.x - k, m.y - k), ImVec2(m.x + k, m.y + k), hv ? colText : colDim, 1.6f);
        dl->AddLine(ImVec2(m.x - k, m.y + k), ImVec2(m.x + k, m.y - k), hv ? colText : colDim, 1.6f);
        if (hv) ImGui::SetTooltip("%s", tr("Close Home (Esc)"));
        if (c) showHome = false;
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false) && !ImGui::IsAnyItemActive()) showHome = false;

    float ry = my + 96 * u;
    text(18 * u, ImVec2(mx, ry), colText, tr("Recent"));
    {   // filter and view toggle
        const float fw = std::min(220 * u, (mr - mx) * 0.4f);
        ImGui::SetCursorScreenPos(ImVec2(mr - fw - 72 * u, ry - 4 * u));
        ImGui::SetNextItemWidth(fw);
        ImGui::InputTextWithHint("##homefilter", tr("Filter recent projects"), homeFilter, sizeof(homeFilter));
        for (int i = 0; i < 2; i++) {
            bool isGrid = i == 0;
            bool hv;
            ImVec2 b0(mr - 64 * u + i * 32 * u, ry - 4 * u);
            bool c = hit(isGrid ? "##homegrid" : "##homelist", b0, ImVec2(30 * u, 26 * u), &hv);
            bool on = homeGrid == isGrid;
            if (on || hv) dl->AddRectFilled(b0, ImVec2(b0.x + 30 * u, b0.y + 26 * u), on ? colRaisedHi : withAlpha(colRaisedHi, 0.6f), 5 * u);
            ImVec2 m(b0.x + 15 * u, b0.y + 13 * u);
            ImU32 ic = on ? colText : colDim;
            if (isGrid) { for (int a = 0; a < 2; a++) for (int b = 0; b < 2; b++) dl->AddRectFilled(ImVec2(m.x - 6 * u + a * 7 * u, m.y - 6 * u + b * 7 * u), ImVec2(m.x - 1 * u + a * 7 * u, m.y - 1 * u + b * 7 * u), ic, 1); }
            else { for (int a = 0; a < 3; a++) dl->AddRectFilled(ImVec2(m.x - 6 * u, m.y - 6 * u + a * 5 * u), ImVec2(m.x + 6 * u, m.y - 4 * u + a * 5 * u), ic, 1); }
            if (hv) ImGui::SetTooltip("%s", isGrid ? tr("Grid view") : tr("List view"));
            if (c) homeGrid = isGrid;
        }
    }

    // scrolling content
    const float areaTop = ry + 34 * u;
    ImGui::SetCursorScreenPos(ImVec2(mx - 8 * u, areaTop));
    ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(0, 0, 0, 0));
    ImGui::BeginChild("##homescroll", ImVec2(mr - mx + 16 * u, pos.y + size.y - areaTop), false, ImGuiWindowFlags_NoBackground);
    ImGui::PopStyleColor();
    ImDrawList* cdl = ImGui::GetWindowDrawList();
    auto ctext = [&](float sz, ImVec2 p, ImU32 c, const char* s, float clipW = 0) {
        if (clipW > 0) { ImVec4 clip(p.x, p.y - 2, p.x + clipW, p.y + sz + 4); cdl->AddText(font, sz, p, c, s, nullptr, 0, &clip); }
        else cdl->AddText(font, sz, p, c, s);
    };
    std::string filter = homeFilter;
    std::transform(filter.begin(), filter.end(), filter.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    std::vector<int> shown;
    for (int i = 0; i < (int)homeItems.size(); i++) {
        std::string low = homeItems[i].name;
        std::transform(low.begin(), low.end(), low.begin(), [](unsigned char c) { return (char)std::tolower(c); });
        if (filter.empty() || low.find(filter) != std::string::npos) shown.push_back(i);
    }
    const ImVec2 origin(mx, ImGui::GetCursorScreenPos().y + 8 * u - ImGui::GetScrollY() * 0);
    const float availW = mr - mx;
    int removeIdx = -1;
    float contentBottom = origin.y;

    auto drawBadge = [&](ImVec2 p, const std::string& ext) {
        std::string s = ext.empty() ? "" : ext.substr(1);
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::toupper(c); });
        float w = textW(10 * u, s.c_str()) + 12 * u;
        cdl->AddRectFilled(p, ImVec2(p.x + w, p.y + 17 * u), IM_COL32(0, 0, 0, 150), 4 * u);
        ctext(10 * u, ImVec2(p.x + 6 * u, p.y + 3 * u), IM_COL32(230, 236, 245, 255), s.c_str());
    };

    if (homeGrid) {
        const float tw = 176 * u, th = 128 * u, gap = 18 * u, rowH = th + 64 * u;
        int cols = std::max(1, (int)((availW + gap) / (tw + gap)));
        int n = 1 + (int)shown.size(); // the first tile is "New project"
        for (int idx = 0; idx < n; idx++) {
            int col = idx % cols, row = idx / cols;
            ImVec2 p0(origin.x + col * (tw + gap), origin.y + row * rowH);
            ImVec2 p1(p0.x + tw, p0.y + th);
            contentBottom = std::max(contentBottom, p0.y + rowH);
            ImGui::PushID(idx);
            ImGui::SetCursorScreenPos(p0);
            bool clicked = ImGui::InvisibleButton("##tile", ImVec2(tw, rowH - 10 * u));
            bool hv = ImGui::IsItemHovered();
            if (hv) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            if (idx == 0) {
                cdl->AddRectFilled(p0, p1, hv ? colRaisedHi : colRaised, 10 * u);
                cdl->AddRect(p0, p1, hv ? colTeal : colBorder, 10 * u, 0, 1.5f);
                ImVec2 m((p0.x + p1.x) / 2, (p0.y + p1.y) / 2 - 4 * u);
                cdl->AddCircleFilled(m, 20 * u, withAlpha(colTeal, 0.18f));
                cdl->AddLine(ImVec2(m.x - 8 * u, m.y), ImVec2(m.x + 8 * u, m.y), colTeal, 2.0f);
                cdl->AddLine(ImVec2(m.x, m.y - 8 * u), ImVec2(m.x, m.y + 8 * u), colTeal, 2.0f);
                ctext(14 * u, ImVec2(p0.x + 2 * u, p1.y + 10 * u), colText, tr("New project"), tw);
                ctext(11 * u, ImVec2(p0.x + 2 * u, p1.y + 31 * u), colDim, tr("Start from an empty rig"), tw);
                if (clicked) { showHome = false; cmdNew(); }
            } else {
                const HomeItem& it = homeItems[shown[idx - 1]];
                ImU32 c0, c1;
                tileColors(it.name, c0, c1);
                cdl->AddRectFilledMultiColor(p0, p1, c0, c0, c1, c1);   // corners are squared by the border below
                cdl->AddRect(p0, p1, hv ? colTeal : colBorder, 10 * u, 0, hv ? 2.0f : 1.0f);
                drawIcon(cdl, Icon::Skeleton, ImVec2((p0.x + p1.x) / 2, (p0.y + p1.y) / 2 - 6 * u), 46 * u, IM_COL32(255, 255, 255, it.exists ? 210 : 70));
                drawBadge(ImVec2(p0.x + 8 * u, p1.y - 25 * u), it.ext);
                if (!it.exists) {
                    cdl->AddRectFilled(p0, p1, IM_COL32(0, 0, 0, 110));
                    float w = textW(12 * u, tr("File not found"));
                    ctext(12 * u, ImVec2((p0.x + p1.x - w) / 2, p0.y + 10 * u), IM_COL32(255, 190, 120, 255), tr("File not found"));
                }
                ctext(14 * u, ImVec2(p0.x + 2 * u, p1.y + 10 * u), it.exists ? colText : colDim, it.name.c_str(), tw);
                ctext(11 * u, ImVec2(p0.x + 2 * u, p1.y + 31 * u), colDim, it.age >= 0 ? ago(it.age).c_str() : "", tw);
                if (hv) ImGui::SetTooltip("%s", it.path.c_str());
                if (clicked) {
                    if (it.exists) { std::string path = it.path; showHome = false; openPath(path); }
                    else setStatus(tr("File not found") + std::string(": ") + it.path);
                }
                if (ImGui::BeginPopupContextItem("##tilemenu")) {
                    if (it.exists && ImGui::MenuItem(tr("Open"))) { std::string path = it.path; showHome = false; openPath(path); }
                    if (ImGui::MenuItem(tr("Remove from list"))) removeIdx = shown[idx - 1];
                    ImGui::EndPopup();
                }
            }
            ImGui::PopID();
        }
        if (homeItems.empty()) {
            int rows = 1;
            ctext(13 * u, ImVec2(origin.x + (tw + gap) , origin.y + th / 2 - 8 * u), colDim, tr("Your recent projects will appear here."));
            contentBottom = origin.y + rows * rowH;
        } else if (shown.empty()) {
            ctext(13 * u, ImVec2(origin.x + (tw + gap), origin.y + th / 2 - 8 * u), colDim, tr("No projects match the filter."));
        }
    } else {
        const float rh = 54 * u;
        int n = (int)shown.size();
        if (n == 0) ctext(13 * u, ImVec2(origin.x, origin.y + 6 * u), colDim, homeItems.empty() ? tr("Your recent projects will appear here.") : tr("No projects match the filter."));
        for (int k = 0; k < n; k++) {
            const HomeItem& it = homeItems[shown[k]];
            ImVec2 p0(origin.x, origin.y + k * (rh + 6 * u)), p1(origin.x + availW, p0.y + rh);
            contentBottom = std::max(contentBottom, p1.y + 6 * u);
            ImGui::PushID(k);
            ImGui::SetCursorScreenPos(p0);
            bool clicked = ImGui::InvisibleButton("##row", ImVec2(availW, rh));
            bool hv = ImGui::IsItemHovered();
            if (hv) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            cdl->AddRectFilled(p0, p1, hv ? colRaisedHi : colRaised, 8 * u);
            ImU32 c0, c1;
            tileColors(it.name, c0, c1);
            ImVec2 t0(p0.x + 8 * u, p0.y + 7 * u), t1(t0.x + 56 * u, t0.y + 40 * u);
            cdl->AddRectFilledMultiColor(t0, t1, c0, c0, c1, c1);
            drawIcon(cdl, Icon::Skeleton, ImVec2((t0.x + t1.x) / 2, (t0.y + t1.y) / 2), 22 * u, IM_COL32(255, 255, 255, it.exists ? 220 : 80));
            ctext(14 * u, ImVec2(t1.x + 14 * u, p0.y + 9 * u), it.exists ? colText : colDim, it.name.c_str(), availW * 0.4f);
            ctext(11 * u, ImVec2(t1.x + 14 * u, p0.y + 30 * u), colDim, it.path.c_str(), availW - 250 * u);
            std::string right = it.exists ? (it.age >= 0 ? ago(it.age) : std::string()) : std::string(tr("File not found"));
            float w = textW(12 * u, right.c_str());
            ctext(12 * u, ImVec2(p1.x - w - 16 * u, p0.y + 19 * u), it.exists ? colDim : IM_COL32(255, 190, 120, 255), right.c_str());
            if (clicked) {
                if (it.exists) { std::string path = it.path; showHome = false; openPath(path); }
                else setStatus(tr("File not found") + std::string(": ") + it.path);
            }
            if (ImGui::BeginPopupContextItem("##rowmenu")) {
                if (it.exists && ImGui::MenuItem(tr("Open"))) { std::string path = it.path; showHome = false; openPath(path); }
                if (ImGui::MenuItem(tr("Remove from list"))) removeIdx = shown[k];
                ImGui::EndPopup();
            }
            ImGui::PopID();
        }
    }
    ImGui::SetCursorScreenPos(ImVec2(origin.x, contentBottom + 12 * u));
    ImGui::Dummy(ImVec2(1, 1)); // lets the child scroll to the bottom of the content
    ImGui::EndChild();
    ImGui::End();
    ImGui::PopStyleVar(); // the fade-in alpha

    if (removeIdx >= 0 && removeIdx < (int)homeItems.size()) {
        std::string p = homeItems[removeIdx].path;
        doc.recentFiles.erase(std::remove(doc.recentFiles.begin(), doc.recentFiles.end(), p), doc.recentFiles.end());
        doc.saveRecent();
        homeDirty = true;
    }
}

} // namespace osteon
