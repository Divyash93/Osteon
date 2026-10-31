// The Export dialog: data (JSON), image (GIF, PNG, JPEG) and video (AVI, MOV) exports, laid out like Spine's.
#include "App.h"
#include "Icons.h"
#include "Util.h"
#include "VideoWriter.h"
#include <sstream>

namespace osteon {

// ---- rendering frames for the image and video exports ----

void Editor::renderFrames(const ExportRender& r, std::vector<std::vector<uint8_t>>& frames, int& w, int& h) {
    Skeleton s(doc.data);
    s.skin = doc.skin;
    s.updateCacheOrder();
    const Animation* a = r.anim >= 0 && r.anim < (int)doc.data.animations.size() ? &doc.data.animations[r.anim] : nullptr;
    const float fps = std::max(1.0f, r.fps);
    int total = r.currentPose || !a ? 1 : std::max(1, (int)std::floor(a->duration() * fps + 0.5f) + 1);
    int from = r.currentPose ? 0 : std::max(0, std::min(r.frameFrom, total - 1));
    int to = r.currentPose ? 0 : (r.frameTo < 0 ? total - 1 : std::max(from, std::min(r.frameTo, total - 1)));
    auto timeOf = [&](int i) { return r.currentPose ? r.poseTime : i / fps; };
    auto pose = [&](int i, float time) {
        s.setToSetupPose();
        if (a) applyAnimation(*a, s, time, time, false, nullptr, 1, MixBlend::Setup);
        s.time = time;
        s.updateWorldTransform(a ? Physics::Update : Physics::None);
        (void)i;
    };
    // Bounds over the frames that will be written.
    float x0 = 1e30f, y0 = 1e30f, x1 = -1e30f, y1 = -1e30f;
    for (int i = from; i <= to; i++) {
        pose(i, timeOf(i));
        float a0, b0, a1, b1;
        if (s.bounds(a0, b0, a1, b1)) { x0 = std::min(x0, a0); y0 = std::min(y0, b0); x1 = std::max(x1, a1); y1 = std::max(y1, b1); }
    }
    if (x0 > x1) { x0 = y0 = -1; x1 = y1 = 1; }
    if (!r.tightCrop && doc.data.width > 0 && doc.data.height > 0) { // the skeleton's own box, widened to hold the pose
        x0 = std::min(x0, doc.data.x); y0 = std::min(y0, doc.data.y);
        x1 = std::max(x1, doc.data.x + doc.data.width); y1 = std::max(y1, doc.data.y + doc.data.height);
    }
    float scale = r.scale;
    const float bw = std::max(1e-3f, x1 - x0), bh = std::max(1e-3f, y1 - y0);
    if (r.sizeMode == 1) scale = std::max(1, r.sizePx - r.pad * 2) / bw;        // fit a pixel width
    else if (r.sizeMode == 2) scale = std::max(1, r.sizePx - r.pad * 2) / bh;   // fit a pixel height
    w = std::max(1, (int)std::ceil(bw * scale) + r.pad * 2);
    h = std::max(1, (int)std::ceil(bh * scale) + r.pad * 2);
    const int k = std::max(1, std::min(4, r.supersample));
    gfx.ensureTarget(exportRT, w * k, h * k);
    RenderOptions opt;
    opt.background = false;
    opt.smoothing = r.smoothing;
    s.physics.assign(s.physics.size(), PhysicsConstraint());
    // Warm up: run the physics for a few steps before the first frame so it does not start from rest.
    for (int i = r.warmup; i >= 1; i--) pose(from, timeOf(from) - i / fps);
    frames.clear();
    for (int i = from; i <= to; i++) {
        pose(i, timeOf(i));
        gfx.begin(exportRT, (x0 + x1) / 2, (y0 + y1) / 2, scale * k, opt);
        if (r.drawImages) gfx.drawSkeleton(s, [this](const Attachment& at, int seq) { return doc.imagePathFor(at, seq); }, opt);
        gfx.end();
        std::vector<uint8_t> big = gfx.readback(exportRT), px;
        if (k == 1) px = std::move(big);
        else { // box filter down to the export size, weighting colours by alpha
            px.assign((size_t)w * h * 4, 0);
            for (int y = 0; y < h; y++)
                for (int x = 0; x < w; x++) {
                    float sr = 0, sg = 0, sb = 0, sa = 0;
                    for (int dy = 0; dy < k; dy++)
                        for (int dx = 0; dx < k; dx++) {
                            const uint8_t* q = &big[((size_t)(y * k + dy) * w * k + (x * k + dx)) * 4];
                            float al = q[3] / 255.0f;
                            sr += q[0] * al; sg += q[1] * al; sb += q[2] * al; sa += al;
                        }
                    uint8_t* o = &px[((size_t)y * w + x) * 4];
                    if (sa > 0) { o[0] = (uint8_t)(sr / sa); o[1] = (uint8_t)(sg / sa); o[2] = (uint8_t)(sb / sa); }
                    o[3] = (uint8_t)(255.0f * sa / (k * k));
                }
        }
        if (!r.transparent)
            for (size_t p = 0; p < px.size(); p += 4) {
                float al = px[p + 3] / 255.0f;
                px[p] = (uint8_t)(px[p] * al + r.bg.r * 255 * (1 - al));
                px[p + 1] = (uint8_t)(px[p + 1] * al + r.bg.g * 255 * (1 - al));
                px[p + 2] = (uint8_t)(px[p + 2] * al + r.bg.b * 255 * (1 - al));
                px[p + 3] = 255;
            }
        frames.push_back(std::move(px));
    }
}

// ---- dialog state ----

namespace {

enum ExportKind { kJson, kBinary, kHtml, kGif, kPng, kApng, kWebp, kAwebp, kJpeg, kAvi, kMov, kWebm, kKindCount };

struct Settings {
    int kind = kJson;
    int exportType = 1;           // 0 current pose, 1 animation, 2 all animations
    std::string folder, file;     // output folder (data, image sequences) and output file (GIF, video)
    int warmup = 1;
    bool renderImages = true;
    int smoothing = 8;            // 0 nearest, 1..10 bilinear 10%..100%
    int msaa = 0;                 // 0 none, 1 = 2x, 2 = 4x
    bool crop = true;
    int sizeMode = 0;             // 0 scale %, 1 width px, 2 height px
    int size = 100;
    bool range = false;
    int rangeFrom = 0, rangeTo = 30;
    int encoding = 0;
    bool transparent = true;
    float bg[3] = {0.2f, 0.2f, 0.2f};
    int compression = 6;
    bool openAfter = false;
    // JSON
    bool pretty = true, nonessential = true, cleanUp = false;
    float jsonScale = 1;
    bool v43 = false;
    char version43[16] = "4.3.26";
};

const char* kindName(int k) {
    static const char* n[] = {"JSON", "Binary", "HTML", "GIF", "PNG", "APNG", "WEBP", "AWEBP", "JPEG", "AVI", "MOV", "WEBM"};
    return n[k];
}

const char* kindExt(int k) {
    static const char* e[] = {".json", ".skel", ".html", ".gif", ".png", ".png", ".webp", ".webp", ".jpg", ".avi", ".mov", ".webm"};
    return e[k];
}

bool isVideo(int k) { return k == kAvi || k == kMov || k == kWebm; }
bool isImage(int k) { return k == kGif || k == kPng || k == kApng || k == kWebp || k == kAwebp || k == kJpeg; }

// Why a format cannot be chosen (empty = available).
std::string unavailable(int k) {
    switch (k) {
    case kBinary: return "Binary (.skel) export is not available: its format has no usable public specification.";
    case kHtml: return "HTML export is not available.";
    case kApng: return "APNG export is not available.";
    case kWebp: case kAwebp: return imageFormatAvailable(ImageFormat::Webp) ? std::string() : "WebP needs Windows' WebP codec, which is not installed.";
    case kWebm: return "WebM needs a VP8/VP9 encoder, which Osteon does not include.";
    default: return {};
    }
}

std::string settingsPath() { return joinPath(appDataDir(), "export_settings.txt"); }

std::string serialize(const Settings& s) {
    std::ostringstream o;
    o << s.kind << ' ' << s.exportType << ' ' << s.warmup << ' ' << s.renderImages << ' ' << s.smoothing << ' ' << s.msaa << ' ' << s.crop << ' '
      << s.sizeMode << ' ' << s.size << ' ' << s.range << ' ' << s.rangeFrom << ' ' << s.rangeTo << ' ' << s.encoding << ' ' << s.transparent << ' '
      << s.bg[0] << ' ' << s.bg[1] << ' ' << s.bg[2] << ' ' << s.compression << ' ' << s.openAfter << ' ' << s.pretty << ' ' << s.nonessential << ' '
      << s.cleanUp << ' ' << s.jsonScale << ' ' << s.v43;
    return o.str();
}

bool deserialize(const std::string& text, Settings& s) {
    std::istringstream in(text);
    Settings t = s;
    int kind;
    if (!(in >> kind >> t.exportType >> t.warmup >> t.renderImages >> t.smoothing >> t.msaa >> t.crop >> t.sizeMode >> t.size >> t.range >> t.rangeFrom >>
          t.rangeTo >> t.encoding >> t.transparent >> t.bg[0] >> t.bg[1] >> t.bg[2] >> t.compression >> t.openAfter >> t.pretty >> t.nonessential >> t.cleanUp >>
          t.jsonScale >> t.v43))
        return false;
    if (kind < 0 || kind >= kKindCount || !unavailable(kind).empty()) kind = kJson;
    t.kind = kind;
    t.exportType = std::max(0, std::min(2, t.exportType));
    t.msaa = std::max(0, std::min(2, t.msaa));
    t.smoothing = std::max(0, std::min(10, t.smoothing));
    t.compression = std::max(1, std::min(9, t.compression));
    t.size = std::max(1, std::min(20000, t.size));
    t.jsonScale = t.jsonScale > 0 ? t.jsonScale : 1;
    s = t;
    return true;
}

bool inputPath(const char* label, std::string& s) {
    char buf[1024];
    strncpy_s(buf, s.c_str(), _TRUNCATE);
    bool r = ImGui::InputText(label, buf, sizeof buf);
    if (r) s = buf;
    return r;
}

const ImVec4 kTeal(0.53f, 0.82f, 0.86f, 1);

} // namespace

void Editor::exportDialog() {
    ImGui::SetNextWindowSize(ImVec2(800 * uiScale, 600 * uiScale), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(760 * uiScale, 560 * uiScale), ImVec2(1e9f, 1e9f)); // an old saved layout cannot leave it too small
    if (!ImGui::Begin("Export", &showExport, ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoCollapse)) { ImGui::End(); return; }
    windowShadow();
    static Settings st;
    static bool loaded = false;
    if (!loaded) {
        loaded = true;
        std::string text;
        if (readTextFile(settingsPath(), text)) deserialize(text, st);
    }
    std::string base = doc.projectPath.empty() ? "skeleton" : stripExtension(fileName(doc.projectPath));
    if (st.folder.empty()) st.folder = doc.projectPath.empty() ? (doc.imagesDir.empty() ? "" : parentDir(doc.imagesDir)) : parentDir(doc.projectPath);
    const float u = uiScale;

    // ---- left: the formats ----
    ImGui::BeginChild("##exptypes", ImVec2(112 * u, -34 * u), ImGuiChildFlags_None);
    auto section = [&](const char* title) { ImGui::Dummy(ImVec2(0, 4 * u)); ImGui::TextColored(kTeal, "%s", title); };
    auto choice = [&](int k) {
        std::string why = unavailable(k);
        ImGui::BeginDisabled(!why.empty());
        if (ImGui::RadioButton(kindName(k), st.kind == k)) {
            st.kind = k;
            st.encoding = 0;
            if (k == kJpeg) st.transparent = false;
            if (k == kAvi) st.transparent = false;
            if (k == kMov) st.transparent = true;
        }
        ImGui::EndDisabled();
        if (!why.empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("%s", why.c_str());
    };
    section("Data");
    choice(kJson); choice(kBinary); choice(kHtml);
    section("Image");
    for (int k : {kGif, kPng, kApng, kWebp, kAwebp, kJpeg}) choice(k);
    section("Video");
    for (int k : {kAvi, kMov, kWebm}) choice(k);
    ImGui::EndChild();
    ImGui::SameLine();

    // ---- right: the options ----
    ImGui::BeginChild("##expopts", ImVec2(0, -34 * u), ImGuiChildFlags_None);
    const int kind = st.kind;
    const bool video = isVideo(kind), json = kind == kJson;
    const float labelW = 116 * u;
    auto label = [&](const char* text) {
        ImGui::AlignTextToFramePadding();
        float tw = ImGui::CalcTextSize(text).x;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, labelW - tw - 8 * u));
        ImGui::TextColored(ImVec4(0.78f, 0.80f, 0.82f, 1), "%s", text);
        ImGui::SameLine(labelW + 4 * u);
    };
    const char* title = kind == kMov ? "Export QuickTime MOV" : kind == kAvi ? "Export AVI" : kind == kJson ? "Export JSON" : kind == kGif ? "Export GIF"
                      : kind == kPng ? "Export PNG sequence" : kind == kJpeg ? "Export JPEG sequence" : "Export";
    ImGui::TextColored(kTeal, "%s", title);
    // Defaults / Preview sit at the right of the title row.
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 170 * u + ImGui::GetCursorPosX());
    if (iconButton("##expdef", Icon::Sync, "Defaults", false, ImVec2(86 * u, 24 * u), "Reset these options")) { Settings keep = st; st = Settings(); st.kind = kind; st.folder = keep.folder; st.file.clear(); if (kind == kAvi || kind == kJpeg) st.transparent = false; }
    ImGui::SameLine();
    ImGui::BeginDisabled();
    iconButton("##exppreview", Icon::Play, "Preview", false, ImVec2(80 * u, 24 * u), "Preview is not available");
    ImGui::EndDisabled();
    ImGui::Separator();
    ImGui::Dummy(ImVec2(0, 4 * u));
    ImGui::PushItemWidth(-60 * u);

    std::string outPath;
    if (json) {
        label("Output folder");
        inputPath("##expfolder", st.folder);
        ImGui::SameLine();
        if (ImGui::Button("...##expf")) { std::string f = pickFolderDialog(hwnd, L"Export folder"); if (!f.empty()) st.folder = f; }
        label("Format");
        ImGui::Checkbox("Pretty print", &st.pretty);
        label("Data");
        ImGui::Checkbox("Nonessential (bone colors, mesh edges, images path)", &st.nonessential);
        label("Clean up");
        ImGui::Checkbox("Animation clean-up (remove keys that repeat their neighbours)", &st.cleanUp);
        label("Scale");
        ImGui::SetNextItemWidth(120 * u);
        ImGui::DragFloat("##expjs", &st.jsonScale, 0.01f, 0.01f, 100, "%.2f");
        label("Version");
        if (ImGui::RadioButton("Spine 4.2 (runtimes)", !st.v43)) st.v43 = false;
        ImGui::SameLine();
        if (ImGui::RadioButton("Spine 4.3 editor", st.v43)) st.v43 = true;
        if (st.v43) { ImGui::SameLine(); ImGui::SetNextItemWidth(80 * u); ImGui::InputText("##expver", st.version43, sizeof st.version43); }
    } else {
        // Export type.
        label("Export type:");
        static const char* types[] = {"Current pose", "Animation", "All animations"};
        ImGui::SetNextItemWidth(160 * u);
        if (ImGui::BeginCombo("##exptype", types[st.exportType])) {
            for (int i = 0; i < 3; i++) if (ImGui::Selectable(types[i], st.exportType == i)) st.exportType = i;
            ImGui::EndCombo();
        }
        // Output: a file for GIF and video, a folder for image sequences.
        const bool singleFile = video || kind == kGif;
        if (singleFile) {
            label("Output file:");
            if (st.file.empty()) st.file = joinPath(st.folder, base + kindExt(kind));
            else if (extension(st.file) != kindExt(kind)) st.file = stripExtension(st.file) + kindExt(kind);
            inputPath("##expfile", st.file);
            ImGui::SameLine();
            if (ImGui::Button("*##expdefault")) st.file = joinPath(st.folder, base + kindExt(kind));
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Default file: next to the project");
            ImGui::SameLine();
            if (ImGui::Button("...##expfile")) {
                std::wstring filter = widen(std::string(kindName(kind)) + " (*" + kindExt(kind) + ")") + L'\0' + widen(std::string("*") + kindExt(kind)) + L'\0' + L"All files" + L'\0' + L"*.*" + L'\0';
                std::string f = saveFileDialog(hwnd, filter.c_str(), widen(std::string(kindExt(kind)).substr(1)).c_str(), st.file, L"Export file");
                if (!f.empty()) st.file = f;
            }
        } else {
            label("Output folder:");
            inputPath("##expfolder", st.folder);
            ImGui::SameLine();
            if (ImGui::Button("...##expf")) { std::string f = pickFolderDialog(hwnd, L"Export folder"); if (!f.empty()) st.folder = f; }
        }
        label("Warm up:");
        sliderInt("##expwarm", &st.warmup, 0, 60, "%d");
        label("Render:");
        ImGui::Checkbox("Images", &st.renderImages);
        for (const char* off : {"Bones", "Others", "Selection", "Titles"}) {
            ImGui::SameLine();
            ImGui::BeginDisabled();
            bool no = false;
            ImGui::Checkbox(off, &no);
            ImGui::EndDisabled();
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("Only images are rendered into exports");
        }
        label("Smoothing:");
        sliderInt("##expsmooth", &st.smoothing, 0, 10, "%d");
        ImGui::SameLine();
        ImGui::TextDisabled(st.smoothing == 0 ? "Nearest" : "Bilinear %d%%", st.smoothing * 10);
        label("Multisample AA:");
        static const char* aa[] = {"None", "2x", "4x"};
        ImGui::SetNextItemWidth(80 * u);
        if (ImGui::BeginCombo("##expaa", aa[st.msaa])) {
            for (int i = 0; i < 3; i++) if (ImGui::Selectable(aa[i], st.msaa == i)) st.msaa = i;
            ImGui::EndCombo();
        }
        label("Viewport:");
        ImGui::Checkbox("Crop", &st.crop);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Checked: the image is cropped tightly to the skeleton. Unchecked: the skeleton's own bounds are kept");
        label("Size:");
        static const char* sizes[] = {"Scale", "Width", "Height"};
        ImGui::SetNextItemWidth(80 * u);
        if (ImGui::BeginCombo("##expsizemode", sizes[st.sizeMode])) {
            for (int i = 0; i < 3; i++) if (ImGui::Selectable(sizes[i], st.sizeMode == i)) { st.sizeMode = i; st.size = i == 0 ? 100 : 512; }
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        sliderInt("##expsize", &st.size, 1, st.sizeMode == 0 ? 400 : 4096, "%d");
        label("Frames:");
        ImGui::Checkbox("Range", &st.range);
        if (st.range) {
            ImGui::SameLine();
            ImGui::SetNextItemWidth(70 * u);
            ImGui::InputInt("##exprf", &st.rangeFrom, 0);
            ImGui::SameLine();
            ImGui::TextDisabled("to");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(70 * u);
            ImGui::InputInt("##exprt", &st.rangeTo, 0);
            st.rangeFrom = std::max(0, st.rangeFrom);
            st.rangeTo = std::max(st.rangeFrom, st.rangeTo);
        }
        ImGui::Separator();
        // Encoding: video only.
        const bool lossy = kind == kJpeg || (kind == kMov && st.encoding == 1) || (kind == kAvi && st.encoding == 1);
        if (video) {
            label("Encoding:");
            const char* enc[2] = {kind == kMov ? "PNG" : "Uncompressed", kind == kMov ? "JPEG" : "MJPEG"};
            ImGui::SetNextItemWidth(130 * u);
            if (ImGui::BeginCombo("##expenc", enc[st.encoding])) {
                for (int i = 0; i < 2; i++) if (ImGui::Selectable(enc[i], st.encoding == i)) st.encoding = i;
                ImGui::EndCombo();
            }
        }
        // Background: transparent where the format keeps alpha.
        const bool alphaOk = kind == kGif || kind == kPng || (kind == kMov && st.encoding == 0);
        label("Background:");
        if (!alphaOk) st.transparent = false;
        ImGui::BeginDisabled(!alphaOk);
        ImGui::Checkbox("Transparent", &st.transparent);
        ImGui::EndDisabled();
        if (!alphaOk && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("This encoding has no alpha channel: the background colour is used");
        if (!st.transparent) { ImGui::SameLine(); ImGui::ColorEdit3("##expbg", st.bg, ImGuiColorEditFlags_NoInputs); }
        if (video || kind == kJpeg) {
            label("Compression:");
            ImGui::BeginDisabled(!lossy);
            sliderInt("##expcomp", &st.compression, 1, 9, "%d");
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::TextDisabled(!lossy ? "Lossless" : st.compression <= 3 ? "Low" : st.compression <= 6 ? "Medium%s" : "High", st.compression == 6 ? " (default)" : "");
        }
        if (video) {
            label("Audio:");
            ImGui::SetNextItemWidth(90 * u);
            if (ImGui::BeginCombo("##expaudio", "None")) { ImGui::Selectable("None", true); ImGui::EndCombo(); }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Audio is not muxed into videos");
        }
    }
    ImGui::PopItemWidth();
    ImGui::EndChild();

    // ---- bottom bar ----
    ImGui::Separator();
    if (ImGui::Button("Save", ImVec2(74 * u, 0))) writeTextFile(settingsPath(), serialize(st));
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Remember these export settings");
    ImGui::SameLine();
    if (ImGui::Button("Load", ImVec2(74 * u, 0))) { std::string text; if (readTextFile(settingsPath(), text) && deserialize(text, st)) setStatus("Export settings loaded"); else setStatus("No saved export settings"); }
    ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 330 * u);
    ImGui::Checkbox("Open after export", &st.openAfter);
    ImGui::SameLine();
    const bool canExport = unavailable(kind).empty();
    ImGui::BeginDisabled(!canExport);
    bool doExport = ImGui::Button("Export", ImVec2(84 * u, 0));
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(84 * u, 0))) showExport = false;
    ImGui::End();
    if (!doExport) return;

    // ---- run ----
    std::string outDir = isVideo(kind) || kind == kGif ? parentDir(st.file) : st.folder;
    if (outDir.empty() || !makeDirs(outDir)) { setStatus("Choose a valid output location"); return; }
    if (json) {
        std::string err, path = joinPath(st.folder, base + ".json");
        if (doc.exportJson(path, st.pretty, st.nonessential, err, st.v43 ? std::string(st.version43) : std::string("4.2.00"), st.jsonScale, st.cleanUp)) setStatus("Exported " + path);
        else setStatus(err);
        if (st.openAfter) openInExplorer(st.folder);
        return;
    }
    std::vector<int> anims; // -1 = the setup pose
    bool currentPose = st.exportType == 0;
    if (doc.data.animations.empty()) anims = {-1};
    else if (st.exportType == 2) for (size_t i = 0; i < doc.data.animations.size(); i++) anims.push_back((int)i);
    else anims = {std::max(0, doc.animation)};
    if (currentPose && doc.mode == Mode::Setup) anims = {-1};
    int written = 0;
    for (int ai : anims) {
        ExportRender r;
        r.anim = ai;
        r.currentPose = currentPose;
        r.poseTime = doc.time;
        r.fps = doc.data.fps > 0 ? doc.data.fps : 30;
        r.scale = st.sizeMode == 0 ? st.size / 100.0f : 1;
        r.sizeMode = st.sizeMode;
        r.sizePx = st.size;
        r.transparent = st.transparent;
        r.bg = Color(st.bg[0], st.bg[1], st.bg[2], 1);
        r.warmup = st.warmup;
        r.frameFrom = st.range ? st.rangeFrom : 0;
        r.frameTo = st.range ? st.rangeTo : -1;
        r.smoothing = st.smoothing == 0 ? 0.0f : 1.0f;
        r.supersample = st.msaa == 0 ? 1 : st.msaa == 1 ? 2 : 4;
        r.drawImages = st.renderImages;
        r.tightCrop = st.crop;
        std::vector<std::vector<uint8_t>> frames;
        int w = 0, h = 0;
        renderFrames(r, frames, w, h);
        std::string name = ai >= 0 ? doc.data.animations[ai].name : "setup";
        auto fileFor = [&](const std::string& suffix) {
            if (st.exportType == 2 && (isVideo(kind) || kind == kGif)) return joinPath(outDir, stripExtension(fileName(st.file)) + "_" + name + kindExt(kind));
            return st.file.empty() ? joinPath(outDir, name + suffix) : st.file;
        };
        const int fps = std::max(1, (int)std::lround(r.fps));
        const int quality = std::max(30, 100 - (st.compression - 1) * 7);
        if (kind == kPng || kind == kJpeg) {
            const bool jpeg = kind == kJpeg;
            for (size_t f = 0; f < frames.size(); f++) {
                char suffix[32];
                std::snprintf(suffix, sizeof suffix, frames.size() > 1 ? "_%04zu%s" : "%s", frames.size() > 1 ? f : 0, jpeg ? ".jpg" : ".png");
                std::string p = joinPath(st.folder, name + (frames.size() > 1 ? std::string(suffix) : std::string(jpeg ? ".jpg" : ".png")));
                if (jpeg ? saveJpegFile(p, w, h, frames[f].data(), quality) : savePngFile(p, w, h, frames[f].data())) written++;
            }
        } else if (kind == kGif) {
            if (saveGifFile(fileFor(".gif"), w, h, frames, std::max(2, (int)std::lround(100.0f / r.fps)))) written++;
        } else if (kind == kAvi) {
            FrameEncoder enc;
            if (st.encoding == 1) enc = [quality](const uint8_t* px, int fw, int fh, std::vector<uint8_t>& o) { return encodeImageBytes(ImageFormat::Jpeg, fw, fh, px, quality, o); };
            if (writeAvi(fileFor(".avi"), w, h, fps, frames, enc)) written++;
        } else if (kind == kMov) {
            const bool jpeg = st.encoding == 1;
            FrameEncoder enc = [jpeg, quality](const uint8_t* px, int fw, int fh, std::vector<uint8_t>& o) {
                return encodeImageBytes(jpeg ? ImageFormat::Jpeg : ImageFormat::Png, fw, fh, px, quality, o);
            };
            if (writeMov(fileFor(".mov"), w, h, fps, frames, enc, jpeg ? "jpeg" : "png ", !jpeg && st.transparent)) written++;
        }
    }
    setStatus(written ? "Exported " + std::to_string(written) + " file(s) to " + outDir : "Export failed");
    if (written && st.openAfter) openInExplorer(outDir);
}

} // namespace osteon
