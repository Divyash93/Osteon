// Named window layouts: which panels are open and how they are docked, saved under a name and loaded, renamed or deleted later.
#pragma once
#include <array>
#include <string>
#include <vector>

namespace osteon {

constexpr int kLayoutPanels = 13; // tree, properties, dopesheet, graph, animations, skins, events, mesh tools, weights, preview, metrics, outline, problems

struct SavedLayout {
    std::array<bool, kLayoutPanels> panels{};
    std::string ini; // Dear ImGui's window / dock layout
};

/** The name as it will be stored: trimmed, internal runs of spaces kept. Empty when it cannot be used
 *  (empty, longer than 48 characters, a character Windows does not allow in file names, or a reserved device name). */
std::string cleanLayoutName(const std::string& name);

/** Folder that holds the layouts, e.g. %APPDATA%\Osteon\layouts. */
std::string layoutsDir();

/** Saved layout names, sorted without regard to case. */
std::vector<std::string> listLayouts(const std::string& dir);
/** True when a layout with that name exists (names are compared without regard to case, as the file system does). */
bool layoutExists(const std::string& dir, const std::string& name);
/** Writes (or replaces) a layout. False for an unusable name or a write failure. */
bool saveLayout(const std::string& dir, const std::string& name, const SavedLayout& layout);
bool loadLayout(const std::string& dir, const std::string& name, SavedLayout& out);
/** Renames a layout. False when `from` does not exist, `to` is unusable, or `to` already belongs to another layout. */
bool renameLayout(const std::string& dir, const std::string& from, const std::string& to);
bool deleteLayout(const std::string& dir, const std::string& name);

/** The file format, exposed for tests. */
std::string serializeLayout(const SavedLayout& layout);
bool parseLayout(const std::string& text, SavedLayout& out);

} // namespace osteon
