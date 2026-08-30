// Lock plugin: touch-lock indicator at the very bottom, below the status
// line. LVGL 8.4's montserrat fonts have no lock glyph (lv_symbol_def.h),
// so the closed eye is the closest built-in symbol. Unlocked shows nothing.
#include "render_color.h"
#include "ui_plugin.h"

static lv_obj_t *sLabel;

static void create(lv_obj_t *screen) {
  sLabel = lv_label_create(screen);
  lv_obj_set_style_text_font(sLabel, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(sLabel, COL_WARN, 0);
  lv_label_set_text(sLabel, "");
  lv_obj_align(sLabel, LV_ALIGN_CENTER, 0, 200);
}

static void update(const DashModel &m) {
  uiSetLabelText(sLabel, m.touchLocked ? LV_SYMBOL_EYE_CLOSE : "");
}

extern const UiPlugin kPluginLock = {"lock", create, update};
