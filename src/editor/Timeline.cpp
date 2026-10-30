// Animate-mode views: playback bar, dopesheet, graph editor and preview.
#include "App.h"
#include "Icons.h"
#include "imgui_internal.h"
#include <set>

namespace osteon {

static ImU32 keyColor(TimelineType t) {
    switch (t) {
    case TimelineType::Rotate: return IM_COL32(110, 220, 110, 255);
    case TimelineType::Translate: case TimelineType::TranslateX: case TimelineType::TranslateY: return IM_COL32(90, 160, 255, 255);
    case TimelineType::Scale: case TimelineType::ScaleX: case TimelineType::ScaleY: return IM_COL32(240, 90, 90, 255);
    case TimelineType::Shear: case TimelineType::ShearX: case TimelineType::ShearY: return IM_COL32(240, 210, 80, 255);
    case TimelineType::Attachment: return IM_COL32(240, 170, 60, 255);
    case TimelineType::RGBA: case TimelineType::RGB: case TimelineType::Alpha: case TimelineType::RGBA2: case TimelineType::RGB2: return IM_COL32(230, 120, 230, 255);
    case TimelineType::Deform: return IM_COL32(170, 120, 255, 255);
    case TimelineType::Event: return IM_COL32(200, 90, 255, 255);
    case TimelineType::DrawOrder: return IM_COL32(120, 230, 230, 255);
    default: return IM_COL32(220, 220, 220, 255);
    }
}

// Icon for each timeline type, as in Spine's dopesheet rows.
static Icon timelineIcon(TimelineType t) {
    switch (t) {
    case TimelineType::Rotate: return Icon::Rotate;
    case TimelineType::Translate: case TimelineType::TranslateX: case TimelineType::TranslateY: return Icon::Translate;
    case TimelineType::Scale: case TimelineType::ScaleX: case TimelineType::ScaleY: return Icon::Scale;
    case TimelineType::Shear: case TimelineType::ShearX: case TimelineType::ShearY: return Icon::Shear;
    case TimelineType::Inherit: return Icon::AxisParent;
    case TimelineType::Attachment: case TimelineType::Sequence: return Icon::Image;
    case TimelineType::RGBA: case TimelineType::RGB: case TimelineType::Alpha: case TimelineType::RGBA2: case TimelineType::RGB2: return Icon::Color;
    case TimelineType::Deform: return Icon::Deform;
    case TimelineType::DrawOrder: return Icon::DrawOrder;
    case TimelineType::Event: return Icon::Events;
    case TimelineType::Ik: return Icon::Ik;
    case TimelineType::Transform: return Icon::TransformCon;
    case TimelineType::Path: case TimelineType::PathMix: return Icon::PathCon;
    case TimelineType::Physics: return Icon::PhysicsCon;
    }
    return Icon::Dot;
}

static std::string timelineLabel(const SkeletonData& d, const Timeline& t) {
    std::string target;
    if (isBoneTimeline(t.type) && t.target >= 0 && t.target < (int)d.bones.size()) target = d.bones[t.target].name;
    else if ((isSlotTimeline(t.type) || t.type == TimelineType::Deform) && t.target >= 0 && t.target < (int)d.slots.size()) target = d.slots[t.target].name;
    else if (t.type == TimelineType::Ik && t.target < (int)d.ik.size()) target = d.ik[t.target].name;
    else if (t.type == TimelineType::Transform && t.target < (int)d.transforms.size()) target = d.transforms[t.target].name;
    else if ((t.type == TimelineType::Path || t.type == TimelineType::PathMix) && t.target < (int)d.paths.size()) target = d.paths[t.target].name;
    else if (t.type == TimelineType::Event) return "Events";
    else if (t.type == TimelineType::DrawOrder) return "Draw order";
    std::string kind = timelineName(t.type);
    if (t.type == TimelineType::Deform) kind = "deform " + t.attachment;
    return target + "  " + kind;
}

std::vector<int> Editor::visibleTimelines() {
    std::vector<int> out;
    Animation* an = doc.currentAnimation();
    if (!an) return out;
    std::vector<int> bones = doc.selectedBones(), slots = doc.selectedSlots();
    bool filter = dopeFilter && (!bones.empty() || !slots.empty());
    for (size_t i = 0; i < an->timelines.size(); i++) {
        const Timeline& t = an->timelines[i];
        if (filter) {
            bool keep = (isBoneTimeline(t.type) && std::find(bones.begin(), bones.end(), t.target) != bones.end()) ||
                        ((isSlotTimeline(t.type) || t.type == TimelineType::Deform) && std::find(slots.begin(), slots.end(), t.target) != slots.end());
            if (!keep) continue;
        }
        out.push_back((int)i);
    }
    // Group by target then type, like the dopesheet hierarchy.
    std::stable_sort(out.begin(), out.end(), [&](int a, int b) {
        const Timeline &x = an->timelines[a], &y = an->timelines[b];
        bool xb = isBoneTimeline(x.type), yb = isBoneTimeline(y.type);
        if (xb != yb) return xb;
        if (x.target != y.target) return x.target < y.target;
        return x.type < y.type;
    });
    return out;
}

void Editor::playbackBar() {
    float fps = doc.data.fps;
    Animation* an = doc.currentAnimation();
    // Animation chooser.
    ImGui::SetNextItemWidth(160 * uiScale);
    if (ImGui::BeginCombo("##anim", an ? an->name.c_str() : "(no animation)")) {
        for (size_t i = 0; i < doc.data.animations.size(); i++)
            if (ImGui::Selectable(doc.data.animations[i].name.c_str(), (int)i == doc.animation)) { doc.animation = (int)i; doc.setTime(0); }
        ImGui::Separator();
        if (ImGui::Selectable("+ New animation")) doc.createAnimation("animation");
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    auto ibtn = [&](const char* id, Icon ic, const char* tip, bool active = false) {
        bool r = iconOnlyButton(id, ic, active, 24 * uiScale, tip);
        ImGui::SameLine(0, 2 * uiScale);
        return r;
    };
    auto btn = [&](const char* l, const char* tip) {
        bool r = ImGui::Button(l);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tip);
        ImGui::SameLine();
        return r;
    };
    std::vector<float> keys;
    if (an) for (auto& t : an->timelines) for (auto& k : t.keys) keys.push_back(k.time);
    std::sort(keys.begin(), keys.end());
    if (ibtn("##first", Icon::FirstKey, "First key (Q)")) doc.setTime(keys.empty() ? 0 : keys.front());
    if (ibtn("##prevkey", Icon::PrevKey, "Previous key (S)")) for (auto it = keys.rbegin(); it != keys.rend(); ++it) if (*it < doc.time - 1e-4f) { doc.setTime(*it); break; }
    if (ibtn("##prevframe", Icon::PrevFrame, "Previous frame (F)")) doc.setTime(std::max(0.0f, std::round(doc.time * fps) - 1) / fps);
    if (ibtn("##pb", Icon::PlayBack, "Play backward (A)", doc.playing && doc.playBackward)) { doc.playBackward = true; doc.playing = !(doc.playing && doc.playBackward); }
    if (ibtn("##pf", doc.playing && !doc.playBackward ? Icon::Pause : Icon::Play, "Play (D)", doc.playing && !doc.playBackward)) { doc.playBackward = false; doc.playing = !(doc.playing && !doc.playBackward); }
    if (ibtn("##nextframe", Icon::NextFrame, "Next frame (R)")) doc.setTime(std::round(doc.time * fps + 1) / fps);
    if (ibtn("##nextkey", Icon::NextKey, "Next key (W)")) for (float t : keys) if (t > doc.time + 1e-4f) { doc.setTime(t); break; }
    if (ibtn("##last", Icon::LastKey, "Last key (E)")) doc.setTime(keys.empty() ? 0 : keys.back());
    if (ibtn("##loop", Icon::Loop, "Repeat playback", doc.loop)) doc.loop = !doc.loop;
    ImGui::SetNextItemWidth(80 * uiScale);
    ImGui::SliderFloat("##speed", &doc.speed, 0.1f, 2, "speed %.2f");
    ImGui::SameLine();
    if (doc.autoKey) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.75f, 0.22f, 0.22f, 1));
    if (btn("Auto Key", "Ctrl+Shift+A: record changes as keys automatically")) doc.autoKey = !doc.autoKey;
    if (doc.autoKey) ImGui::PopStyleColor();
    if (doc.loopStart >= 0 || doc.loopEnd >= 0) {
        if (btn("Clear loop", "Loop range set with [ and ]")) doc.loopStart = doc.loopEnd = -1;
    }
    ImGui::NewLine();
}

// Sets curve type for a key; Bezier presets use handles at 1/3 and 2/3 of the segment.
static void setCurve(Timeline& tl, int ki, CurveType type, float easeIn = 1.0f / 3, float easeOut = 2.0f / 3, bool flat = true) {
    Key& k = tl.keys[ki];
    k.curve = type;
    k.bezier.clear();
    if (type != CurveType::Bezier || ki + 1 >= (int)tl.keys.size()) {
        if (type == CurveType::Bezier) k.curve = CurveType::Linear;
        return;
    }
    const Key& n = tl.keys[ki + 1];
    int vc = std::max(1, timelineValueCount(tl.type));
    for (int v = 0; v < vc; v++) {
        float v0 = tl.type == TimelineType::Deform ? 0 : k.v[v], v1 = tl.type == TimelineType::Deform ? 1 : n.v[v];
        float dt = n.time - k.time;
        k.bezier.push_back(k.time + dt * easeIn);
        k.bezier.push_back(flat ? v0 : v0 + (v1 - v0) * easeIn);
        k.bezier.push_back(k.time + dt * easeOut);
        k.bezier.push_back(flat ? v1 : v0 + (v1 - v0) * easeOut);
    }
}

// Top row of the dopesheet (Spine's Sync / Filter / Lock / Ghosting / Shift group and the frame fields).
void Editor::dopesheetToolbar() {
    const float fps = doc.data.fps;
    auto tool = [&](const char* id, Icon ic, const char* tip, bool active) {
        bool r = iconOnlyButton(id, ic, active, 24 * uiScale, tip);
        ImGui::SameLine(0, 2 * uiScale);
        return r;
    };
    if (tool("##sync", Icon::Sync, "Sync: re-apply the pose at the current frame", false)) doc.updatePose();
    if (tool("##filter", Icon::Filter, "Filter: show only the timelines of the selected bones and slots", dopeFilter)) dopeFilter = !dopeFilter;
    if (tool("##lock", Icon::Lock, "Lock: prevent key edits in the dopesheet", dopeLocked)) dopeLocked = !dopeLocked;
    if (tool("##ghost", Icon::Eye, "Ghosting (I): show previous and next frames", ghosting)) ghosting = !ghosting;
    ImGui::SameLine(0, 10 * uiScale);

    // Key tools on the selected keys: copy, cut, paste, delete, and the curve type.
    Animation* an = doc.currentAnimation();
    bool hasSel = an && !selKeys.empty() && !dopeLocked;
    auto copyKeys = [&]() {
        keyClipboard.clear();
        float t0 = 1e9f;
        for (auto& k : selKeys) t0 = std::min(t0, an->timelines[k.timeline].keys[k.key].time);
        for (auto& k : selKeys) {
            Timeline shell = an->timelines[k.timeline];
            shell.keys = {an->timelines[k.timeline].keys[k.key]};
            keyClipboard.push_back({shell, {shell.keys[0].time - t0}});
        }
        setStatus(std::to_string(keyClipboard.size()) + " keys copied");
    };
    auto deleteKeys = [&]() {
        doc.checkpoint("Delete keys");
        an = doc.currentAnimation();
        std::sort(selKeys.begin(), selKeys.end(), [](const KeyRef& a, const KeyRef& b) { return a.timeline != b.timeline ? a.timeline > b.timeline : a.key > b.key; });
        for (auto& k : selKeys) an->timelines[k.timeline].keys.erase(an->timelines[k.timeline].keys.begin() + k.key);
        an->timelines.erase(std::remove_if(an->timelines.begin(), an->timelines.end(), [](const Timeline& t) { return t.keys.empty(); }), an->timelines.end());
        selKeys.clear();
    };
    ImGui::BeginDisabled(!hasSel);
    if (tool("##copy", Icon::Copy, "Copy the selected keys (Ctrl+C)", false)) copyKeys();
    if (tool("##cut", Icon::Cut, "Cut the selected keys", false)) { copyKeys(); deleteKeys(); }
    ImGui::EndDisabled();
    ImGui::BeginDisabled(!an || keyClipboard.empty() || dopeLocked);
    if (tool("##paste", Icon::Paste, "Paste keys at the current frame (Ctrl+V)", false)) {
        doc.checkpoint("Paste keys");
        an = doc.currentAnimation();
        for (auto& [shell, rel] : keyClipboard) {
            Timeline& tl = an->get(shell.type, shell.target, shell.skin, shell.attachment);
            float t = std::round((doc.time + rel[0]) * fps) / fps;
            Key k = shell.keys[0];
            float shift = t - k.time;
            k.time = t;
            for (size_t b = 0; b < k.bezier.size(); b += 2) k.bezier[b] += shift;
            tl.setKey(t) = k;
        }
        selKeys.clear();
    }
    ImGui::EndDisabled();
    ImGui::BeginDisabled(!hasSel);
    if (tool("##delkeys", Icon::Delete, "Delete the selected keys (Del)", false)) deleteKeys();
    ImGui::EndDisabled();
    ImGui::SameLine(0, 6 * uiScale);
    auto curveButton = [&](const char* id, Icon ic, const char* tip, CurveType type) {
        ImGui::BeginDisabled(!hasSel);
        if (tool(id, ic, tip, false)) {
            doc.checkpoint("Curve");
            an = doc.currentAnimation();
            for (auto& k : selKeys) setCurve(an->timelines[k.timeline], k.key, type, 1.0f / 3, 2.0f / 3, true);
        }
        ImGui::EndDisabled();
    };
    curveButton("##clin", Icon::CurveLinear, "Linear curve for the selected keys", CurveType::Linear);
    curveButton("##cbez", Icon::CurveBezier, "Bezier curve for the selected keys", CurveType::Bezier);
    curveButton("##cstep", Icon::CurveStepped, "Stepped curve for the selected keys", CurveType::Stepped);
    ImGui::SameLine(0, 10 * uiScale);
    if (tool("##shift", Icon::Shift, "Shift Keys: drag the selected keys in time", !dopeOffset)) dopeOffset = false;
    if (tool("##offset", Icon::Translate, "Offset Keys: dragging a key also moves the keys after it", dopeOffset)) dopeOffset = true;
    ImGui::BeginDisabled();
    tool("##adjust", Icon::Adjust, "Adjust: not implemented yet", false);
    ImGui::EndDisabled();
    ImGui::SameLine(0, 10 * uiScale);

    ImGui::SetNextItemWidth(70 * uiScale);
    int cur = (int)std::round(doc.time * fps);
    if (ImGui::InputInt("Current", &cur, 0)) doc.setTime(std::max(0, cur) / fps);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(70 * uiScale);
    int ls = doc.loopStart >= 0 ? (int)std::round(doc.loopStart * fps) : 0;
    if (ImGui::InputInt("Loop Start", &ls, 0)) doc.loopStart = std::max(0, ls) / fps;
    ImGui::SameLine();
    ImGui::SetNextItemWidth(70 * uiScale);
    int le = doc.loopEnd >= 0 ? (int)std::round(doc.loopEnd * fps) : (int)std::round(doc.animationDuration() * fps);
    if (ImGui::InputInt("End", &le, 0)) doc.loopEnd = std::max(0, le) / fps;
}

void Editor::dopesheetView() {
    ImGui::Begin("Dopesheet", &showDopesheet);
    if (doc.mode != Mode::Animate) {
        ImGui::TextDisabled("Switch to Animate mode (Ctrl+Tab or the SETUP label) to edit animations.");
        if (ImGui::Button("Animate mode")) doc.setMode(Mode::Animate);
        ImGui::End();
        return;
    }
    dopesheetToolbar();
    playbackBar();
    Animation* an = doc.currentAnimation();
    if (!an) {
        ImGui::TextDisabled("No animation. Create one to start keying.");
        if (ImGui::Button("New animation")) doc.createAnimation("animation");
        ImGui::End();
        return;
    }
    float fps = doc.data.fps;
    std::vector<int> rows = visibleTimelines();
    float labelW = 200 * uiScale, rowH = ImGui::GetTextLineHeight() + 6, rulerH = 22 * uiScale;
    ImVec2 origin = ImGui::GetCursorScreenPos();
    ImVec2 avail = ImGui::GetContentRegionAvail();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    float canvasX = origin.x + labelW;
    float canvasW = avail.x - labelW;
    if (canvasW < 50) { ImGui::End(); return; }
    auto frameToX = [&](float f) { return canvasX + (f - tlScroll) * tlZoom; };
    auto xToFrame = [&](float x) { return (x - canvasX) / tlZoom + tlScroll; };

    // Background, ruler, loop range.
    dl->AddRectFilled(ImVec2(canvasX, origin.y), ImVec2(canvasX + canvasW, origin.y + avail.y), IM_COL32(28, 28, 28, 255));
    dl->AddRectFilled(ImVec2(canvasX, origin.y), ImVec2(canvasX + canvasW, origin.y + rulerH), IM_COL32(40, 40, 40, 255));
    int step = tlZoom >= 10 ? 1 : tlZoom >= 4 ? 5 : 10;
    int labelStep = tlZoom >= 20 ? 5 : tlZoom >= 8 ? 10 : 20;
    float durFrames = an->duration() * fps;
    for (int f = (int)std::max(0.0f, std::floor(tlScroll)); frameToX((float)f) < canvasX + canvasW; f++) {
        if (f % step) continue;
        float x = frameToX((float)f);
        bool major = f % labelStep == 0;
        dl->AddLine(ImVec2(x, origin.y + (major ? 6 : 14)), ImVec2(x, origin.y + rulerH), IM_COL32(120, 120, 120, 255));
        if (major) dl->AddText(ImVec2(x + 2, origin.y + 2), IM_COL32(170, 170, 170, 255), std::to_string(f).c_str());
        dl->AddLine(ImVec2(x, origin.y + rulerH), ImVec2(x, origin.y + avail.y), major ? IM_COL32(48, 48, 48, 255) : IM_COL32(36, 36, 36, 255));
    }
    if (durFrames > 0) dl->AddRectFilled(ImVec2(frameToX(durFrames), origin.y + rulerH), ImVec2(canvasX + canvasW, origin.y + avail.y), IM_COL32(0, 0, 0, 60));
    if (doc.loopStart >= 0) dl->AddLine(ImVec2(frameToX(doc.loopStart * fps), origin.y), ImVec2(frameToX(doc.loopStart * fps), origin.y + avail.y), IM_COL32(80, 200, 80, 255), 2);
    if (doc.loopEnd >= 0) dl->AddLine(ImVec2(frameToX(doc.loopEnd * fps), origin.y), ImVec2(frameToX(doc.loopEnd * fps), origin.y + avail.y), IM_COL32(200, 80, 80, 255), 2);

    // Ruler scrubbing.
    ImGui::SetCursorScreenPos(ImVec2(canvasX, origin.y));
    ImGui::InvisibleButton("##ruler", ImVec2(canvasW, rulerH));
    if (ImGui::IsItemActive()) { doc.playing = false; doc.setTime(std::max(0.0f, std::round(xToFrame(ImGui::GetMousePos().x))) / fps); }

    // Rows.
    static bool dragKeys = false, boxSel = false;
    static ImVec2 boxStart;
    static float dragStartFrame = 0;
    static std::vector<std::pair<KeyRef, float>> dragOrig;
    ImGuiIO& io = ImGui::GetIO();
    bool canvasHovered = ImGui::IsMouseHoveringRect(ImVec2(canvasX, origin.y + rulerH), ImVec2(canvasX + canvasW, origin.y + avail.y)) && ImGui::IsWindowHovered();
    float y = origin.y + rulerH - tlRowScroll;
    int hitTimeline = -1, hitKey = -1;
    dl->PushClipRect(ImVec2(origin.x, origin.y + rulerH), ImVec2(origin.x + avail.x, origin.y + avail.y), true);
    // Group the timelines by bone or slot (types without a target group by type), with a folding header row each.
    auto groupKey = [&](const Timeline& tl) -> std::string {
        if (isBoneTimeline(tl.type)) return "b" + std::to_string(tl.target);
        if (isSlotTimeline(tl.type) || tl.type == TimelineType::Deform) return "s" + std::to_string(tl.target);
        return std::string("o") + timelineName(tl.type);
    };
    struct DRow { bool header; int ti; std::string key; };
    std::vector<DRow> layout;
    std::map<std::string, std::vector<int>> groupTimelines;
    std::string lastKey;
    for (int ti : rows) {
        std::string key = groupKey(an->timelines[ti]);
        groupTimelines[key].push_back(ti);
        if (key != lastKey) { layout.push_back({true, -1, key}); lastKey = key; }
        if (!collapsedGroups.count(key)) layout.push_back({false, ti, key});
    }
    auto groupTitle = [&](int ti, Icon& ic, ImU32& col) -> std::string {
        const Timeline& tl = an->timelines[ti];
        if (isBoneTimeline(tl.type)) {
            const Color& c = doc.data.bones[tl.target].color;
            ic = Icon::Bone;
            col = ImGui::ColorConvertFloat4ToU32(ImVec4(c.r, c.g, c.b, 1));
            return doc.data.bones[tl.target].name;
        }
        if (isSlotTimeline(tl.type) || tl.type == TimelineType::Deform) {
            ic = Icon::Slot;
            col = IM_COL32(120, 190, 240, 255);
            return doc.data.slots[tl.target].name;
        }
        ic = timelineIcon(tl.type);
        col = keyColor(tl.type);
        return timelineName(tl.type);
    };
    for (const DRow& e : layout) {
        if (e.header) {
            Icon gi = Icon::Bone;
            ImU32 gc = IM_COL32(255, 255, 255, 255);
            std::string title = groupTitle(groupTimelines[e.key].front(), gi, gc);
            bool closed = collapsedGroups.count(e.key) > 0;
            ImVec2 hmin(origin.x, y), hmax(origin.x + labelW, y + rowH);
            if (canvasHovered == false && ImGui::IsWindowHovered() && ImGui::IsMouseHoveringRect(hmin, hmax) && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
                if (closed) collapsedGroups.erase(e.key); else collapsedGroups.insert(e.key);
            }
            dl->AddLine(ImVec2(origin.x, y), ImVec2(origin.x + avail.x, y), IM_COL32(60, 60, 60, 255));
            float is = rowH * 0.62f;
            drawIcon(dl, closed ? Icon::GroupClosed : Icon::GroupOpen, ImVec2(origin.x + 6 + is / 2, y + rowH / 2), is * 0.8f, IM_COL32(200, 200, 200, 255));
            drawIcon(dl, gi, ImVec2(origin.x + 6 + is + 10 + is / 2, y + rowH / 2), is, gc);
            dl->AddText(ImVec2(origin.x + 6 + is * 2 + 14, y + 3), IM_COL32(235, 235, 235, 255), title.c_str());
            // Summary of the group's keys, so folded groups still show where their keys are.
            for (int gt : groupTimelines[e.key])
                for (auto& k : an->timelines[gt].keys) {
                    float x = frameToX(k.time * fps), cy = y + rowH / 2;
                    if (x < canvasX - 8 || x > canvasX + canvasW + 8) continue;
                    dl->AddRectFilled(ImVec2(x - 2, cy - 2), ImVec2(x + 2, cy + 2), IM_COL32(170, 170, 170, 255));
                }
            y += rowH;
            continue;
        }
        int ti = e.ti;
        const Timeline& tl = an->timelines[ti];
        bool rowSel = false;
        for (auto& k : selKeys) if (k.timeline == ti) rowSel = true;
        if (rowSel) dl->AddRectFilled(ImVec2(origin.x, y), ImVec2(origin.x + labelW, y + rowH), IM_COL32(40, 70, 100, 255));
        float iconSize = rowH * 0.62f, indent = 6 + iconSize * 2 + 14;
        drawIcon(dl, timelineIcon(tl.type), ImVec2(origin.x + indent + iconSize / 2, y + rowH / 2), iconSize, keyColor(tl.type));
        std::string kind = tl.type == TimelineType::Deform ? "deform " + tl.attachment : std::string(timelineName(tl.type));
        dl->AddText(ImVec2(origin.x + indent + iconSize + 6, y + 3), IM_COL32(210, 210, 210, 255), kind.c_str());
        for (size_t k = 0; k < tl.keys.size(); k++) {
            float x = frameToX(tl.keys[k].time * fps), cy = y + rowH / 2;
            if (x < canvasX - 8 || x > canvasX + canvasW + 8) continue;
            bool sel = std::find(selKeys.begin(), selKeys.end(), KeyRef{ti, (int)k}) != selKeys.end();
            float r = 5;
            ImVec2 pts[4] = {ImVec2(x, cy - r), ImVec2(x + r, cy), ImVec2(x, cy + r), ImVec2(x - r, cy)};
            dl->AddConvexPolyFilled(pts, 4, sel ? IM_COL32(255, 255, 255, 255) : keyColor(tl.type));
            dl->AddPolyline(pts, 4, IM_COL32(0, 0, 0, 255), ImDrawFlags_Closed, 1.0f);
            if (tl.keys[k].curve == CurveType::Stepped && k + 1 < tl.keys.size())
                dl->AddLine(ImVec2(x + r, cy), ImVec2(frameToX(tl.keys[k + 1].time * fps) - r, cy), IM_COL32(120, 120, 120, 200));
            if (canvasHovered && std::fabs(io.MousePos.x - x) <= r + 1 && std::fabs(io.MousePos.y - cy) <= r + 1) { hitTimeline = ti; hitKey = (int)k; }
        }
        y += rowH;
    }
    if (rows.empty())
        dl->AddText(ImVec2(origin.x + 8, y + 6), IM_COL32(140, 140, 140, 255),
                    doc.selection.empty() ? "No keys yet. Pose bones with Auto Key on, or press K/L to key." : "No keys for the selection.");
    // Current time line.
    float tx = frameToX(doc.time * fps);
    dl->PopClipRect();
    dl->AddLine(ImVec2(tx, origin.y), ImVec2(tx, origin.y + avail.y), IM_COL32(60, 170, 255, 255), 2);
    dl->AddTriangleFilled(ImVec2(tx - 6, origin.y), ImVec2(tx + 6, origin.y), ImVec2(tx, origin.y + 8), IM_COL32(60, 170, 255, 255));

    // Key interaction.
    if (canvasHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        bool add = io.KeyCtrl || io.KeyShift;
        if (hitKey >= 0) {
            KeyRef r{hitTimeline, hitKey};
            auto it = std::find(selKeys.begin(), selKeys.end(), r);
            if (add) { if (it == selKeys.end()) selKeys.push_back(r); else selKeys.erase(it); }
            else if (it == selKeys.end()) selKeys = {r};
            doc.playing = false;
            doc.setTime(an->timelines[hitTimeline].keys[hitKey].time);
            dragKeys = !dopeLocked;
            dragStartFrame = xToFrame(io.MousePos.x);
            dragOrig.clear();
            if (dopeOffset) { // Offset: the selected keys and every later key in the visible timelines move together
                float from = 1e9f;
                for (auto& k : selKeys) from = std::min(from, an->timelines[k.timeline].keys[k.key].time);
                for (int ti : rows) {
                    const Timeline& tl = an->timelines[ti];
                    for (size_t k = 0; k < tl.keys.size(); k++)
                        if (tl.keys[k].time >= from - 1e-4f) dragOrig.push_back({KeyRef{ti, (int)k}, tl.keys[k].time});
                }
            } else {
                for (auto& k : selKeys) dragOrig.push_back({k, an->timelines[k.timeline].keys[k.key].time});
            }
            if (!dopeLocked) doc.beginEdit("Move keys");
        } else {
            if (!add) selKeys.clear();
            boxSel = true;
            boxStart = io.MousePos;
        }
    }
    if (dragKeys) {
        float df = std::round(xToFrame(io.MousePos.x) - dragStartFrame);
        float minT = 1e9f;
        for (auto& [k, t] : dragOrig) minT = std::min(minT, t);
        if (minT + df / fps < 0) df = -std::round(minT * fps);
        for (auto& [k, t] : dragOrig) {
            Key& key = an->timelines[k.timeline].keys[k.key];
            float nt = t + df / fps;
            float shift = nt - key.time;
            key.time = nt;
            for (size_t b = 0; b < key.bezier.size(); b += 2) key.bezier[b] += shift;
        }
        if (df != 0) doc.dirty = true;
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            dragKeys = false;
            // Re-sort keys; when two keys land on the same frame the moved one wins.
            std::set<int> touched;
            for (auto& [k, t] : dragOrig) touched.insert(k.timeline);
            for (int ti : touched) {
                Timeline& tl = an->timelines[ti];
                std::vector<bool> moved(tl.keys.size(), false);
                for (auto& [k, t] : dragOrig) if (k.timeline == ti) moved[k.key] = true;
                std::vector<std::pair<Key, bool>> ks;
                for (size_t i = 0; i < tl.keys.size(); i++) ks.push_back({tl.keys[i], moved[i]});
                std::stable_sort(ks.begin(), ks.end(), [](auto& a, auto& b) { return a.first.time < b.first.time; });
                std::vector<Key> out;
                for (auto& [k, m] : ks) {
                    if (!out.empty() && std::fabs(out.back().time - k.time) < 1e-4f) { if (m) out.back() = k; continue; }
                    out.push_back(k);
                }
                tl.keys = out;
            }
            selKeys.clear();
            if (df != 0) doc.commitEdit(); else doc.cancelEdit();
        }
    }
    if (boxSel) {
        ImVec2 a = boxStart, b = io.MousePos;
        dl->AddRect(ImVec2(std::min(a.x, b.x), std::min(a.y, b.y)), ImVec2(std::max(a.x, b.x), std::max(a.y, b.y)), IM_COL32(90, 210, 255, 255));
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            boxSel = false;
            float x0 = std::min(a.x, b.x), x1 = std::max(a.x, b.x), y0 = std::min(a.y, b.y), y1 = std::max(a.y, b.y);
            float ry = origin.y + rulerH - tlRowScroll;
            for (const DRow& e : layout) {
                if (!e.header) {
                    const Timeline& tl = an->timelines[e.ti];
                    float cy = ry + rowH / 2;
                    for (size_t k = 0; k < tl.keys.size(); k++) {
                        float x = frameToX(tl.keys[k].time * fps);
                        if (x >= x0 && x <= x1 && cy >= y0 && cy <= y1 && std::find(selKeys.begin(), selKeys.end(), KeyRef{e.ti, (int)k}) == selKeys.end())
                            selKeys.push_back({e.ti, (int)k});
                    }
                }
                ry += rowH;
            }
            if (std::fabs(a.x - b.x) < 3 && std::fabs(a.y - b.y) < 3) { doc.playing = false; doc.setTime(std::max(0.0f, std::round(xToFrame(a.x))) / fps); }
        }
    }
    // Context menu for keys.
    if (canvasHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        if (hitKey >= 0 && std::find(selKeys.begin(), selKeys.end(), KeyRef{hitTimeline, hitKey}) == selKeys.end()) selKeys = {{hitTimeline, hitKey}};
        ImGui::OpenPopup("##keyctx");
    }
    if (ImGui::BeginPopup("##keyctx")) {
        bool any = !selKeys.empty();
        auto applyCurve = [&](CurveType t, float a, float b, bool flat) {
            doc.checkpoint("Curve");
            an = doc.currentAnimation();
            for (auto& k : selKeys) setCurve(an->timelines[k.timeline], k.key, t, a, b, flat);
        };
        if (ImGui::MenuItem("Linear", nullptr, false, any)) applyCurve(CurveType::Linear, 0, 0, false);
        if (ImGui::MenuItem("Stepped", nullptr, false, any)) applyCurve(CurveType::Stepped, 0, 0, false);
        if (ImGui::MenuItem("Ease In-Out (Bezier)", nullptr, false, any)) applyCurve(CurveType::Bezier, 1.0f / 3, 2.0f / 3, true);
        if (ImGui::MenuItem("Ease Out", nullptr, false, any)) applyCurve(CurveType::Bezier, 0.25f, 0.6f, false);
        if (ImGui::MenuItem("Ease In", nullptr, false, any)) applyCurve(CurveType::Bezier, 0.4f, 0.75f, false);
        ImGui::Separator();
        if (ImGui::MenuItem("Copy", "Ctrl+C", false, any)) {
            keyClipboard.clear();
            float t0 = 1e9f;
            for (auto& k : selKeys) t0 = std::min(t0, an->timelines[k.timeline].keys[k.key].time);
            for (auto& k : selKeys) {
                Timeline shell = an->timelines[k.timeline];
                shell.keys = {an->timelines[k.timeline].keys[k.key]};
                keyClipboard.push_back({shell, {shell.keys[0].time - t0}});
            }
        }
        if (ImGui::MenuItem("Paste at current frame", "Ctrl+V", false, !keyClipboard.empty())) {
            doc.checkpoint("Paste keys");
            an = doc.currentAnimation();
            for (auto& [shell, rel] : keyClipboard) {
                Timeline& tl = an->get(shell.type, shell.target, shell.skin, shell.attachment);
                float t = std::round((doc.time + rel[0]) * fps) / fps;
                Key k = shell.keys[0];
                float shift = t - k.time;
                k.time = t;
                for (size_t b = 0; b < k.bezier.size(); b += 2) k.bezier[b] += shift;
                tl.setKey(t) = k;
            }
        }
        if (ImGui::MenuItem("Delete", "Del", false, any)) {
            doc.checkpoint("Delete keys");
            an = doc.currentAnimation();
            std::sort(selKeys.begin(), selKeys.end(), [](const KeyRef& a, const KeyRef& b) { return a.timeline != b.timeline ? a.timeline > b.timeline : a.key > b.key; });
            for (auto& k : selKeys) an->timelines[k.timeline].keys.erase(an->timelines[k.timeline].keys.begin() + k.key);
            an->timelines.erase(std::remove_if(an->timelines.begin(), an->timelines.end(), [](const Timeline& t) { return t.keys.empty(); }), an->timelines.end());
            selKeys.clear();
        }
        ImGui::EndPopup();
    }
    // Ctrl+C / Ctrl+V when the dopesheet is focused.
    if (ImGui::IsWindowFocused() && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_C) && !selKeys.empty()) {
        keyClipboard.clear();
        float t0 = 1e9f;
        for (auto& k : selKeys) t0 = std::min(t0, an->timelines[k.timeline].keys[k.key].time);
        for (auto& k : selKeys) {
            Timeline shell = an->timelines[k.timeline];
            shell.keys = {an->timelines[k.timeline].keys[k.key]};
            keyClipboard.push_back({shell, {shell.keys[0].time - t0}});
        }
        setStatus(std::to_string(keyClipboard.size()) + " keys copied");
    }
    if (ImGui::IsWindowFocused() && io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_V) && !keyClipboard.empty()) {
        doc.checkpoint("Paste keys");
        an = doc.currentAnimation();
        for (auto& [shell, rel] : keyClipboard) {
            Timeline& tl = an->get(shell.type, shell.target, shell.skin, shell.attachment);
            float t = std::round((doc.time + rel[0]) * fps) / fps;
            Key k = shell.keys[0];
            float shift = t - k.time;
            k.time = t;
            for (size_t b = 0; b < k.bezier.size(); b += 2) k.bezier[b] += shift;
            tl.setKey(t) = k;
        }
        selKeys.clear();
    }
    // Zoom (Ctrl+wheel) and scroll (wheel).
    if (canvasHovered && io.MouseWheel != 0) {
        if (io.KeyCtrl) {
            float f = xToFrame(io.MousePos.x);
            tlZoom = clampf(tlZoom * std::pow(1.2f, io.MouseWheel), 1, 80);
            tlScroll = f - (io.MousePos.x - canvasX) / tlZoom;
        } else if (io.KeyShift) tlScroll = std::max(0.0f, tlScroll - io.MouseWheel * 5);   // Shift+wheel: frames
        else tlRowScroll = std::max(0.0f, tlRowScroll - io.MouseWheel * rowH * 2);        // wheel: rows
    }
    if (canvasHovered && ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) tlScroll = std::max(0.0f, tlScroll - io.MouseDelta.x / tlZoom);
    tlScroll = std::max(0.0f, tlScroll);
    // Keep selected keys valid after edits elsewhere.
    selKeys.erase(std::remove_if(selKeys.begin(), selKeys.end(), [&](const KeyRef& k) {
        return k.timeline >= (int)an->timelines.size() || k.key >= (int)an->timelines[k.timeline].keys.size(); }), selKeys.end());
    // Scroll bars: long animations scroll across frames, and long key lists scroll down the rows.
    const float hBarH = 12 * uiScale, vBarW = 12 * uiScale;
    float lastKeyFrame = 0;
    for (int ti : rows)
        for (auto& k : an->timelines[ti].keys) lastKeyFrame = std::max(lastKeyFrame, k.time * fps);
    float trackW = canvasW - vBarW;
    float totalFrames = std::max(durFrames, lastKeyFrame) + 30;
    float visFrames = trackW / tlZoom;
    float maxScroll = std::max(0.0f, totalFrames - visFrames);
    float thumbW = trackW * clampf(visFrames / totalFrames, 0.05f, 1.0f);
    float hy = origin.y + avail.y - hBarH;
    ImGui::SetCursorScreenPos(ImVec2(canvasX, hy));
    ImGui::InvisibleButton("##dsHBar", ImVec2(trackW, hBarH));
    if (ImGui::IsItemActive() && trackW - thumbW > 0) tlScroll = clampf((io.MousePos.x - canvasX - thumbW / 2) / (trackW - thumbW), 0, 1) * maxScroll;
    float thumbX = canvasX + (maxScroll > 0 ? std::min(tlScroll / maxScroll, 1.0f) : 0) * (trackW - thumbW);
    dl->AddRectFilled(ImVec2(canvasX, hy), ImVec2(canvasX + trackW, hy + hBarH), IM_COL32(40, 40, 40, 255));
    dl->AddRectFilled(ImVec2(thumbX, hy + 2), ImVec2(thumbX + thumbW, hy + hBarH - 2), IM_COL32(110, 110, 118, 255), 4);

    float contentH = layout.size() * rowH;
    float visH = avail.y - rulerH - hBarH;
    float maxRow = std::max(0.0f, contentH - visH);
    tlRowScroll = clampf(tlRowScroll, 0, maxRow);
    float thumbH = visH * clampf(visH / std::max(contentH, 1.0f), 0.05f, 1.0f);
    float vy = origin.y + rulerH;
    float vx = canvasX + trackW;
    ImGui::SetCursorScreenPos(ImVec2(vx, vy));
    ImGui::InvisibleButton("##dsVBar", ImVec2(vBarW, visH));
    if (ImGui::IsItemActive() && visH - thumbH > 0) tlRowScroll = clampf((io.MousePos.y - vy - thumbH / 2) / (visH - thumbH), 0, 1) * maxRow;
    float thumbY = vy + (maxRow > 0 ? tlRowScroll / maxRow : 0) * (visH - thumbH);
    dl->AddRectFilled(ImVec2(vx, vy), ImVec2(vx + vBarW, vy + visH), IM_COL32(40, 40, 40, 255));
    dl->AddRectFilled(ImVec2(vx + 2, thumbY), ImVec2(vx + vBarW - 2, thumbY + thumbH), IM_COL32(110, 110, 118, 255), 4);

    ImGui::SetCursorScreenPos(ImVec2(origin.x, origin.y + avail.y - 1));
    ImGui::Dummy(ImVec2(1, 1));
    ImGui::End();
}

void Editor::graphView() {
    ImGui::Begin("Graph", &showGraph);
    Animation* an = doc.mode == Mode::Animate ? doc.currentAnimation() : nullptr;
    if (!an) {
        ImGui::TextDisabled("The graph shows animation curves in Animate mode.");
        ImGui::End();
        return;
    }
    float fps = doc.data.fps;
    // Which timelines: those with selected keys, else visible ones (curve timelines only).
    std::vector<int> tls;
    for (auto& k : selKeys) if (std::find(tls.begin(), tls.end(), k.timeline) == tls.end()) tls.push_back(k.timeline);
    if (tls.empty()) tls = visibleTimelines();
    tls.erase(std::remove_if(tls.begin(), tls.end(), [&](int t) { return t >= (int)an->timelines.size() || timelineValueCount(an->timelines[t].type) == 0; }), tls.end());

    // Curve presets for selected keys.
    static std::vector<int> hiddenCh; // ti * 8 + channel
    auto isHidden = [&](int ti, int v) { return std::find(hiddenCh.begin(), hiddenCh.end(), ti * 8 + v) != hiddenCh.end(); };
    auto preset = [&](const char* id, Icon ic, const char* tip, CurveType t, float a, float b, bool flat) {
        ImGui::BeginDisabled(selKeys.empty() || dopeLocked);
        if (iconOnlyButton(id, ic, false, 24 * uiScale, tip)) {
            doc.checkpoint("Curve");
            an = doc.currentAnimation();
            for (auto& k : selKeys) setCurve(an->timelines[k.timeline], k.key, t, a, b, flat);
        }
        ImGui::EndDisabled();
        ImGui::SameLine(0, 2 * uiScale);
    };
    preset("##glin", Icon::CurveLinear, "Linear curve for the selected keys", CurveType::Linear, 0, 0, false);
    preset("##gbez", Icon::CurveBezier, "Bezier curve for the selected keys", CurveType::Bezier, 1.0f / 3, 2.0f / 3, true);
    preset("##gstep", Icon::CurveStepped, "Stepped curve for the selected keys", CurveType::Stepped, 0, 0, false);
    ImGui::SameLine(0, 8 * uiScale);
    auto ease = [&](const char* l, float a, float b) {
        ImGui::BeginDisabled(selKeys.empty() || dopeLocked);
        if (ImGui::SmallButton(l)) {
            doc.checkpoint("Curve");
            an = doc.currentAnimation();
            for (auto& k : selKeys) setCurve(an->timelines[k.timeline], k.key, CurveType::Bezier, a, b, false);
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
    };
    ease("Ease In", 0.4f, 0.75f);
    ease("Ease Out", 0.25f, 0.6f);
    ImGui::SameLine(0, 8 * uiScale);
    if (iconOnlyButton("##gframe", Icon::Fit, false, 24 * uiScale, "Frame: fit the whole animation in the view")) {
        tlScroll = 0;
        tlZoom = clampf((ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX()) * 0.6f / std::max(1.0f, an->duration() * fps), 1, 80);
    }
    if (tls.empty()) { ImGui::NewLine(); ImGui::TextDisabled("No curve timelines for the selection."); ImGui::End(); return; }

    // Channel list on the left: one row per curve with a visibility dot, as in Spine.
    const float listW = 150 * uiScale;
    ImGui::BeginChild("##glist", ImVec2(listW, 0), true);
    for (int ti : tls) {
        const Timeline& tl = an->timelines[ti];
        ImGui::TextUnformatted(timelineLabel(doc.data, tl).c_str());
        int vc = timelineValueCount(tl.type);
        static const char* names2[] = {"X", "Y"};
        static const char* names4[] = {"R", "G", "B", "A"};
        for (int v = 0; v < vc; v++) {
            ImGui::PushID(ti * 8 + v);
            bool hid = isHidden(ti, v);
            ImVec2 p = ImGui::GetCursorScreenPos();
            float h = ImGui::GetTextLineHeight();
            ImGui::InvisibleButton("##vis", ImVec2(listW - 16, h + 2));
            if (ImGui::IsItemClicked()) {
                if (hid) hiddenCh.erase(std::find(hiddenCh.begin(), hiddenCh.end(), ti * 8 + v));
                else hiddenCh.push_back(ti * 8 + v);
            }
            ImU32 col = vc == 1 ? keyColor(tl.type) : IM_COL32(240, 90, 90, 255);
            static const ImU32 cc[7] = {IM_COL32(240, 90, 90, 255), IM_COL32(90, 220, 90, 255), IM_COL32(90, 150, 255, 255), IM_COL32(220, 220, 220, 255),
                                        IM_COL32(240, 170, 60, 255), IM_COL32(200, 120, 255, 255), IM_COL32(120, 230, 230, 255)};
            if (vc != 1) col = cc[v % 7];
            ImDrawList* ldl = ImGui::GetWindowDrawList();
            ImVec2 c(p.x + 8, p.y + h * 0.5f + 1);
            if (hid) ldl->AddCircle(c, 4, col); else ldl->AddCircleFilled(c, 4, col);
            const char* nm = vc == 2 ? names2[v] : vc == 4 ? names4[v] : "";
            ldl->AddText(ImVec2(p.x + 20, p.y + 1), hid ? IM_COL32(130, 130, 130, 255) : IM_COL32(220, 220, 220, 255), nm[0] ? nm : "Value");
            ImGui::PopID();
        }
    }
    ImGui::EndChild();
    ImGui::SameLine(0, 0);

    ImVec2 origin = ImGui::GetCursorScreenPos();
    ImVec2 size = ImGui::GetContentRegionAvail();
    if (size.x < 50 || size.y < 50) { ImGui::End(); return; }
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(origin, ImVec2(origin.x + size.x, origin.y + size.y), IM_COL32(28, 28, 28, 255));
    // Value range across shown channels.
    float vmin = 1e30f, vmax = -1e30f, tmax = std::max(an->duration(), 1.0f / fps);
    for (int ti : tls) {
        const Timeline& tl = an->timelines[ti];
        int vc = timelineValueCount(tl.type);
        for (int s = 0; s <= 200; s++)
            for (int v = 0; v < vc; v++) {
                float val = timelineValue(tl, v, tmax * s / 200);
                vmin = std::min(vmin, val); vmax = std::max(vmax, val);
            }
        for (auto& k : tl.keys) for (size_t b = 1; b < k.bezier.size(); b += 2) { vmin = std::min(vmin, k.bezier[b]); vmax = std::max(vmax, k.bezier[b]); }
    }
    if (vmax - vmin < 1e-3f) { vmin -= 1; vmax += 1; }
    float pad = (vmax - vmin) * 0.1f;
    vmin -= pad; vmax += pad;
    auto X = [&](float t) { return origin.x + 10 + (t * fps - tlScroll) * tlZoom; };
    auto Y = [&](float v) { return origin.y + size.y - 10 - (v - vmin) / (vmax - vmin) * (size.y - 20); };
    auto Tinv = [&](float x) { return ((x - origin.x - 10) / tlZoom + tlScroll) / fps; };
    auto Vinv = [&](float y) { return vmin + (origin.y + size.y - 10 - y) / (size.y - 20) * (vmax - vmin); };
    dl->PushClipRect(origin, ImVec2(origin.x + size.x, origin.y + size.y), true);
    if (vmin < 0 && vmax > 0) dl->AddLine(ImVec2(origin.x, Y(0)), ImVec2(origin.x + size.x, Y(0)), IM_COL32(70, 70, 70, 255));
    dl->AddLine(ImVec2(X(doc.time), origin.y), ImVec2(X(doc.time), origin.y + size.y), IM_COL32(60, 170, 255, 255), 2);
    char lbl[32];
    // Value axis on the right: grid lines with labels.
    for (int g = 0; g <= 5; g++) {
        float val = vmin + (vmax - vmin) * g / 5;
        dl->AddLine(ImVec2(origin.x, Y(val)), ImVec2(origin.x + size.x, Y(val)), IM_COL32(42, 42, 42, 255));
        std::snprintf(lbl, sizeof lbl, "%.1f", val);
        dl->AddText(ImVec2(origin.x + size.x - 48, Y(val) - 14), IM_COL32(140, 140, 140, 255), lbl);
    }
    // Frame ruler along the top.
    int fStep = tlZoom >= 20 ? 5 : tlZoom >= 8 ? 10 : 20;
    dl->AddRectFilled(origin, ImVec2(origin.x + size.x, origin.y + 16), IM_COL32(44, 46, 45, 255));
    for (int f = (int)(std::floor(tlScroll / fStep)) * fStep; X(f / fps) < origin.x + size.x; f += fStep) {
        if (f < 0) continue;
        float x = X(f / fps);
        dl->AddLine(ImVec2(x, origin.y + 10), ImVec2(x, origin.y + size.y), IM_COL32(40, 40, 40, 255));
        std::snprintf(lbl, sizeof lbl, "%d", f);
        dl->AddText(ImVec2(x + 2, origin.y), IM_COL32(170, 170, 170, 255), lbl);
    }
    for (int ti : tls)
        for (auto& k : an->timelines[ti].keys) {
            float x = X(k.time);
            dl->AddTriangleFilled(ImVec2(x - 3, origin.y + 16), ImVec2(x + 3, origin.y + 16), ImVec2(x, origin.y + 11), IM_COL32(240, 150, 40, 255));
        }
    static const ImU32 chanCol[7] = {IM_COL32(240, 90, 90, 255), IM_COL32(90, 220, 90, 255), IM_COL32(90, 150, 255, 255), IM_COL32(220, 220, 220, 255),
                                     IM_COL32(240, 170, 60, 255), IM_COL32(200, 120, 255, 255), IM_COL32(120, 230, 230, 255)};
    ImGuiIO& io = ImGui::GetIO();
    static int dragTl = -1, dragKey = -1, dragHandle = -1, dragVal = -1; // handle 0/1 = bezier handles, 2 = key value
    static ImVec2 dragMouse;
    static float dragStartValue = 0;
    bool hovered = ImGui::IsWindowHovered() && ImGui::IsMouseHoveringRect(origin, ImVec2(origin.x + size.x, origin.y + size.y));
    for (int ti : tls) {
        Timeline& tl = an->timelines[ti];
        int vc = timelineValueCount(tl.type);
        for (int v = 0; v < vc; v++) {
            if (isHidden(ti, v)) continue;
            ImU32 col = vc == 1 ? keyColor(tl.type) : chanCol[v % 7];
            float t0 = tl.keys.front().time, t1 = tl.keys.back().time;
            ImVec2 prev;
            int N = std::max(2, (int)((X(t1) - X(t0)) / 3));
            for (int s = 0; s <= N; s++) {
                float t = t0 + (t1 - t0) * s / N;
                ImVec2 p(X(t), Y(timelineValue(tl, v, t)));
                if (s) dl->AddLine(prev, p, col, 1.5f);
                prev = p;
            }
            for (size_t k = 0; k < tl.keys.size(); k++) {
                Key& key = tl.keys[k];
                ImVec2 kp(X(key.time), Y(key.v[v]));
                bool sel = std::find(selKeys.begin(), selKeys.end(), KeyRef{ti, (int)k}) != selKeys.end();
                if (key.curve == CurveType::Bezier && k + 1 < tl.keys.size() && (int)key.bezier.size() >= v * 4 + 4 && sel) {
                    ImVec2 h1(X(key.bezier[v * 4]), Y(key.bezier[v * 4 + 1])), h2(X(key.bezier[v * 4 + 2]), Y(key.bezier[v * 4 + 3]));
                    ImVec2 np(X(tl.keys[k + 1].time), Y(tl.keys[k + 1].v[v]));
                    dl->AddLine(kp, h1, IM_COL32(200, 200, 200, 160));
                    dl->AddLine(np, h2, IM_COL32(200, 200, 200, 160));
                    dl->AddCircleFilled(h1, 4, IM_COL32(255, 255, 255, 255));
                    dl->AddCircleFilled(h2, 4, IM_COL32(255, 255, 255, 255));
                    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && dragTl < 0) {
                        if (std::hypot(io.MousePos.x - h1.x, io.MousePos.y - h1.y) < 6) { dragTl = ti; dragKey = (int)k; dragHandle = 0; dragVal = v; doc.beginEdit("Edit curve"); }
                        else if (std::hypot(io.MousePos.x - h2.x, io.MousePos.y - h2.y) < 6) { dragTl = ti; dragKey = (int)k; dragHandle = 1; dragVal = v; doc.beginEdit("Edit curve"); }
                    }
                }
                dl->AddCircleFilled(kp, sel ? 5.0f : 4.0f, sel ? IM_COL32(255, 255, 255, 255) : col);
                if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && dragTl < 0 && std::hypot(io.MousePos.x - kp.x, io.MousePos.y - kp.y) < 6) {
                    KeyRef r{ti, (int)k};
                    if (!io.KeyCtrl) selKeys = {r};
                    else if (std::find(selKeys.begin(), selKeys.end(), r) == selKeys.end()) selKeys.push_back(r);
                    dragTl = ti; dragKey = (int)k; dragHandle = 2; dragVal = v;
                    dragMouse = io.MousePos;
                    dragStartValue = key.v[v];
                    doc.beginEdit("Edit key value");
                }
            }
        }
    }
    if (dragTl >= 0 && dragTl < (int)an->timelines.size()) {
        Timeline& tl = an->timelines[dragTl];
        if (dragKey < (int)tl.keys.size()) {
            Key& key = tl.keys[dragKey];
            float t = Tinv(io.MousePos.x), v = Vinv(io.MousePos.y);
            // A click selects; only a real drag changes the value, by the mouse's relative motion.
            bool movedEnough = std::fabs(io.MousePos.y - dragMouse.y) > 2;
            if (dragHandle == 2) { if (movedEnough) key.v[dragVal] = dragStartValue + (v - Vinv(dragMouse.y)); }
            else if (dragKey + 1 < (int)tl.keys.size()) {
                // Handles stay within the segment in time so the curve remains a function of time.
                float a = key.time, b = tl.keys[dragKey + 1].time;
                key.bezier[dragVal * 4 + dragHandle * 2] = clampf(t, a, b);
                key.bezier[dragVal * 4 + dragHandle * 2 + 1] = v;
            }
            if (dragHandle != 2 || movedEnough) doc.dirty = true;
        }
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            if (dragHandle == 2 && std::fabs(io.MousePos.y - dragMouse.y) <= 2) doc.cancelEdit(); else doc.commitEdit();
            dragTl = -1;
        }
    }
    if (hovered && io.MouseWheel != 0 && io.KeyCtrl) tlZoom = clampf(tlZoom * std::pow(1.2f, io.MouseWheel), 1, 80);
    // Pan the frames: Shift+wheel, or drag with the middle button.
    if (hovered && io.MouseWheel != 0 && io.KeyShift && !io.KeyCtrl) tlScroll = std::max(0.0f, tlScroll - io.MouseWheel * 5);
    if (hovered && ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) tlScroll = std::max(0.0f, tlScroll - io.MouseDelta.x / tlZoom);
    dl->PopClipRect();
    // The playhead: a marker on the ruler, and the ruler itself is draggable to move the current frame (as in the
    // dopesheet).
    {
        float px = X(doc.time);
        if (px >= origin.x && px <= origin.x + size.x) {
            dl->AddTriangleFilled(ImVec2(px - 6, origin.y), ImVec2(px + 6, origin.y), ImVec2(px, origin.y + 12), IM_COL32(60, 170, 255, 255));
            dl->AddLine(ImVec2(px, origin.y + 12), ImVec2(px, origin.y + 16), IM_COL32(60, 170, 255, 255), 2);
        }
        ImGui::SetCursorScreenPos(origin);
        ImGui::InvisibleButton("##gruler", ImVec2(size.x, 16 * uiScale));
        if (ImGui::IsItemActive()) {
            doc.playing = false;
            doc.setTime(std::max(0.0f, std::round((io.MousePos.x - origin.x - 10) / tlZoom + tlScroll)) / fps);
        }
        if (ImGui::IsItemHovered() || ImGui::IsItemActive()) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    }
    ImGui::SetCursorScreenPos(origin);
    ImGui::Dummy(size);
    ImGui::End();
}

void Editor::previewView(float dt) {
    ImGui::Begin("Preview", &showPreview);
    SkeletonData& d = doc.data;
    if (previewSk.needsRebuild(d)) { previewSk.build(d); previewState = AnimationState(&d); previewAnim = -1; }
    previewState.data = &d;
    ImGui::SetNextItemWidth(160);
    if (ImGui::BeginCombo("Animation", previewAnim >= 0 && previewAnim < (int)d.animations.size() ? d.animations[previewAnim].name.c_str() : "(setup pose)")) {
        for (size_t i = 0; i < d.animations.size(); i++)
            if (ImGui::Selectable(d.animations[i].name.c_str(), (int)i == previewAnim)) {
                previewAnim = (int)i;
                previewState.defaultMix = previewMix;
                previewState.setAnimation(0, previewAnim, previewLoop);
            }
        ImGui::EndCombo();
    }
    ImGui::SameLine(); ImGui::SetNextItemWidth(90); ImGui::SliderFloat("Mix", &previewMix, 0, 1, "%.2fs");
    ImGui::SameLine(); ImGui::SetNextItemWidth(90); ImGui::SliderFloat("Speed", &previewSpeed, 0.1f, 2);
    ImGui::SameLine(); ImGui::Checkbox("Loop", &previewLoop);
    previewSk.skin = doc.skin;
    previewState.timeScale = previewSpeed;
    previewState.update(dt);
    previewSk.setToSetupPose();
    previewState.apply(previewSk);
    previewSk.update(dt * previewSpeed);
    previewSk.updateWorldTransform(Physics::Update);
    ImVec2 size = ImGui::GetContentRegionAvail();
    if (size.x > 10 && size.y > 10) {
        gfx.ensureTarget(previewRT, (int)size.x, (int)size.y);
        float x0, y0, x1, y1;
        Skeleton setup(d);
        setup.updateWorldTransform(Physics::None);
        if (!setup.bounds(x0, y0, x1, y1)) { x0 = y0 = -100; x1 = y1 = 100; }
        float z = std::min(size.x / std::max(1.0f, x1 - x0), size.y / std::max(1.0f, y1 - y0)) * 0.8f;
        RenderOptions opt = renderOptions();
        opt.slotVisible = nullptr;
        gfx.begin(previewRT, (x0 + x1) / 2, (y0 + y1) / 2, z, opt);
        gfx.drawSkeleton(previewSk, [this](const Attachment& a, int seq) { return doc.imagePathFor(a, seq); }, opt);
        gfx.end();
        ImGui::Image((ImTextureID)previewRT.srv, size);
    }
    ImGui::End();
}

} // namespace osteon
