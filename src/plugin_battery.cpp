// Battery plugin: charge state + percent at the top of the dial.
#include <stdio.h>

#include "render_color.h"
#include "ui_plugin.h"

static lv_obj_t *sLabel;

static void create(lv_obj_t *screen) {
  sLabel = lv_label_create(screen);
  lv_obj_set_style_text_font(sLabel, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(sLabel, COL_DIM, 0);
  lv_label_set_text(sLabel, "");
  lv_obj_align(sLabel, LV_ALIGN_CENTER, 0, -150);
}

static void update(const DashModel &m) {
  const PowerStatus &power = m.power;
  if (!power.pmicOk) {
    uiSetLabelText(sLabel, "");
    return;
  }
  if (!power.battConnected) {
    uiSetLabelText(sLabel, LV_SYMBOL_USB);
    lv_obj_set_style_text_color(sLabel, COL_DIM, 0);
    return;
  }
  const char *sym = power.charging          ? LV_SYMBOL_CHARGE
                    : power.percent > 75    ? LV_SYMBOL_BATTERY_FULL
                    : power.percent > 50    ? LV_SYMBOL_BATTERY_3
                    : power.percent > 25    ? LV_SYMBOL_BATTERY_2
                    : power.percent > 10    ? LV_SYMBOL_BATTERY_1
                                            : LV_SYMBOL_BATTERY_EMPTY;
  char buf[24];
  snprintf(buf, sizeof(buf), "%s %d%%", sym, power.percent);
  uiSetLabelText(sLabel, buf);
  lv_obj_set_style_text_color(sLabel, colorBattery(power), 0);
}

extern const UiPlugin kPluginBattery = {"battery", create, update};
