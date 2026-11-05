// Translation tables. One row per English label: Spanish, French, German, Brazilian Portuguese, Hindi, Simplified Chinese, Japanese.
// An empty string means "not translated yet": the English text is shown. Rows live in tr_*.inc so they can be reviewed per area.
#include "I18n.h"

namespace osteon {

extern const TrRow kTrRows[] = {
#include "tr_menus.inc"
#include "tr_panels.inc"
#include "tr_props.inc"
#include "tr_settings.inc"
#include "tr_misc.inc"
#include "tr_more.inc"
#include "tr_home.inc"
};
extern const size_t kTrRowCount = sizeof kTrRows / sizeof kTrRows[0];

} // namespace osteon
