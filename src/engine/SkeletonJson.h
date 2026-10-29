// Reading and writing Spine-format skeleton JSON (reads 3.8 and 4.x, writes 4.2).
#pragma once
#include "Model.h"

namespace osteon {

struct JsonResult {
    bool ok = false;
    std::string error;
    std::vector<std::string> warnings;
};

/** Parses JSON text into `out`. */
JsonResult readSkeletonJson(const std::string& text, SkeletonData& out);

struct JsonWriteOptions {
    bool pretty = true;
    bool nonessential = true; // bone colors, edges, mesh width/height, images path, fps
    std::string editorBlock;  // raw JSON object stored under "osteon" (project files only)
    /** Target format: "4.2.00" (runtimes) or a 4.3 version such as "4.3.26". The Spine editor only
        imports data whose version matches its own; 4.3 moves constraints into one "constraints" array. */
    std::string version = "4.2.00";
};

std::string writeSkeletonJson(const SkeletonData& data, const JsonWriteOptions& opt = {});

/** Extracts the raw "osteon" editor block (or empty) from project JSON text. */
std::string readEditorBlock(const std::string& text);

} // namespace osteon
