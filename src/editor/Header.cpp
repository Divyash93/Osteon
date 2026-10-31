// The custom title bar: the window has no native caption, so this bar carries the logo, the menus, quick icons and the
// minimize / maximize / close buttons, and the empty parts of it drag the window (see WM_NCHITTEST in main.cpp).
#include "App.h"
#include "Icons.h"
#include "Util.h"

namespace osteon {

static const ImVec4 kBarBg(0.157f, 0.192f, 0.227f, 1);

void Editor::headerMarkHot() {
    ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
    headerHot.push_back(RECT{(LONG)a.x, (LONG)a.y, (LONG)b.x, (LONG)b.y});
}

bool Editor::headerInteractiveAt(int x, int y) const {
    for (const RECT& r : headerHot) if (x >= r.left && x < r.right && y >= r.top && y < r.bottom) return true;
    return false;
}

bool Editor::headerMenu(const char* name) {
    // Tall enough for the bar while the title is drawn; the menu's own items keep the normal compact padding.
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(13 * uiScale, (headerHeight - ImGui::GetFontSize()) / 2));
    bool open = ImGui::BeginMenu(name);
    ImGui::PopStyleVar();
    headerMarkHot();
    return open;
}

void Editor::headerSeparator() {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    const float u = uiScale;
    ImGui::Dummy(ImVec2(10 * u, 1));
    ImGui::SameLine(0, 0);
    float x = p.x + 5 * u, top = ImGui::GetMainViewport()->Pos.y;
    dl->AddLine(ImVec2(x, top + 8 * u), ImVec2(x, top + headerHeight - 8 * u), IM_COL32(18, 22, 27, 255), 1.0f);
    dl->AddLine(ImVec2(x + 1, top + 8 * u), ImVec2(x + 1, top + headerHeight - 8 * u), IM_COL32(60, 72, 84, 255), 1.0f);
}

bool Editor::headerIcon(const char* id, Icon icon, const char* tooltip) {
    const float u = uiScale, size = 28 * u;
    ImGui::SetCursorPosY((headerHeight - size) / 2);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
    setIconMono(true, ImGui::GetStyle().Alpha < 0.99f ? IM_COL32(120, 130, 140, 255) : IM_COL32(246, 248, 250, 255));
    bool clicked = iconOnlyButton(id, icon, false, size, tooltip, IM_COL32(246, 248, 250, 255));
    setIconMono(false);
    ImGui::PopStyleColor();
    headerMarkHot();
    ImGui::SameLine(0, 2 * u);
    return clicked;
}

void Editor::headerWindowButtons() {
    const float u = uiScale, bw = 46 * u, h = headerHeight;
    ImGuiViewport* vp = ImGui::GetMainViewport();
    float x0 = vp->Pos.x + vp->Size.x - 3 * bw, y0 = vp->Pos.y;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const bool maximized = IsZoomed(hwnd) != 0;
    for (int i = 0; i < 3; i++) {
        ImVec2 a(x0 + i * bw, y0), b(a.x + bw, y0 + h);
        ImGui::SetCursorScreenPos(a);
        ImGui::PushID(i);
        ImGui::InvisibleButton("##wb", ImVec2(bw, h));
        bool hov = ImGui::IsItemHovered(), down = ImGui::IsItemActive(), clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
        ImGui::PopID();
        headerMarkHot();
        ImU32 fill = i == 2 ? (down ? IM_COL32(180, 30, 40, 255) : hov ? IM_COL32(232, 17, 35, 255) : 0) : (down ? IM_COL32(70, 82, 94, 255) : hov ? IM_COL32(54, 66, 78, 255) : 0);
        if (fill) dl->AddRectFilled(a, b, fill);
        dl->AddLine(ImVec2(a.x, a.y), ImVec2(a.x, b.y), IM_COL32(18, 22, 27, 255), 1.0f); // cell borders, as in the reference
        ImVec2 c((a.x + b.x) / 2, (a.y + b.y) / 2);
        const float s = 6 * u, t = 1.6f * std::max(1.0f, u);
        ImU32 white = IM_COL32(255, 255, 255, 255);
        if (i == 0) dl->AddLine(ImVec2(c.x - s, c.y + s * 0.6f), ImVec2(c.x + s, c.y + s * 0.6f), white, t * 1.4f);
        else if (i == 1) {
            if (maximized) { // restore: two overlapping squares
                dl->AddRect(ImVec2(c.x - s, c.y - s * 0.4f), ImVec2(c.x + s * 0.5f, c.y + s), white, 0, 0, t);
                dl->AddLine(ImVec2(c.x - s * 0.4f, c.y - s * 0.4f), ImVec2(c.x - s * 0.4f, c.y - s), white, t);
                dl->AddLine(ImVec2(c.x - s * 0.4f, c.y - s), ImVec2(c.x + s, c.y - s), white, t);
                dl->AddLine(ImVec2(c.x + s, c.y - s), ImVec2(c.x + s, c.y + s * 0.5f), white, t);
                dl->AddLine(ImVec2(c.x + s, c.y + s * 0.5f), ImVec2(c.x + s * 0.5f, c.y + s * 0.5f), white, t);
            } else dl->AddRect(ImVec2(c.x - s, c.y - s), ImVec2(c.x + s, c.y + s), white, 0, 0, t);
        } else {
            dl->AddLine(ImVec2(c.x - s, c.y - s), ImVec2(c.x + s, c.y + s), white, t * 1.3f);
            dl->AddLine(ImVec2(c.x - s, c.y + s), ImVec2(c.x + s, c.y - s), white, t * 1.3f);
        }
        if (clicked) {
            if (i == 0) ShowWindow(hwnd, SW_MINIMIZE);
            else if (i == 1) ShowWindow(hwnd, maximized ? SW_RESTORE : SW_MAXIMIZE);
            else PostMessageW(hwnd, WM_CLOSE, 0, 0);
        }
    }
    dl->AddLine(ImVec2(x0, y0 + h - 1), ImVec2(x0 + 3 * bw, y0 + h - 1), IM_COL32(18, 22, 27, 255), 1.0f);
}

} // namespace osteon
