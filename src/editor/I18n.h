// User interface language: tr() looks a label up in the translation tables (English text is the key and the fallback).
#pragma once
#include <string>

namespace osteon {

enum class Lang { English, Spanish, French, German, Portuguese, Hindi, Chinese, Japanese, Count };

struct LangInfo {
    const char* code;   // settings.txt value, e.g. "es"
    const char* native; // the language's own name, shown in the picker
};
const LangInfo& langInfo(Lang l);
Lang langFromCode(const std::string& code);

/** One row of the translation tables (Translations.cpp): the English text and its translations, "" = not translated. */
struct TrRow {
    const char* en;
    const char* t[(int)Lang::Count - 1]; // es, fr, de, pt-BR, hi, zh-CN, ja
};
extern const TrRow kTrRows[];
extern const size_t kTrRowCount;

void setLanguage(Lang l);
Lang language();

/** Translates a widget label or text. A "##id" suffix is kept, so ImGui ids stay unique. Unknown text is returned unchanged. */
const char* tr(const char* english);
/** Window title that keeps one ImGui id in every language ("Translated###English"), so saved docking layouts survive a language change. */
const char* trWindow(const char* english);

/** Rewrites window names of a saved layout (older files used plain names) to the stable "Name###Name" form. */
std::string migrateLayoutIni(const std::string& ini);
/** ImGui::LoadIniSettingsFromMemory after migrateLayoutIni. */
void loadLayoutIni(const std::string& ini);

/** The language stored in settings.txt (read before the editor exists, to choose the font order). */
Lang storedLanguage();

/** How many strings the tables translate for `l`, and how many labels the editor looks up in total. */
int translatedCount(Lang l);
int totalLabelCount();

} // namespace osteon
