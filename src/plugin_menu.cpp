// Pull-down menu plugin: swipe down from the top of the dial to open a dark
// settings panel (currently a brightness slider; the flex column is ready
// for more rows). Close by tapping the dimmed background outside the panel
// or the close button.
//
// Open gesture: a dedicated transparent drag zone hugging the top edge
// instead of LVGL's screen-level LV_EVENT_GESTURE. On this layout the screen
// is dense with widgets and the round bezel clips the top corners; a fixed
// zone with a plain "finger moved N px down while pressed" rule is
// deterministic — no dependence on LVGL's gesture velocity threshold or on
// which child object claimed the press.
//
// This plugin is registered LAST in ui.cpp so its objects are created on top
// of every other plugin; open() additionally calls lv_obj_move_foreground as
// a belt-and-braces measure. Other plugins only ever touch their own
// objects, so nothing wipes the panel while it is shown. update() is a
// no-op: the panel only changes state on touch events.
#include <stdio.h>

#include "display_ctl.h"
#include "render_color.h"
#include "ui_plugin.h"

static lv_obj_t *sScrim;   // dimmed full-screen backdrop, tap to close
static lv_obj_t *sPanel;   // the menu panel itself
static lv_obj_t *sZone;    // invisible top-edge drag zone that opens it
static lv_obj_t *sValueLabel;

static void menuOpen() {
  if (!lv_obj_has_flag(sPanel, LV_OBJ_FLAG_HIDDEN)) return;
  lv_obj_clear_flag(sScrim, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(sPanel, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(sScrim);
  lv_obj_move_foreground(sPanel);
}

static void menuClose() {
  if (lv_obj_has_flag(sPanel, LV_OBJ_FLAG_HIDDEN)) return;
  lv_obj_add_flag(sScrim, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_flag(sPanel, LV_OBJ_FLAG_HIDDEN);
}

// Drag zone: open once the finger has pulled >40 px downward.
static void zoneEventCb(lv_event_t *e) {
  static lv_coord_t sPressY;
  lv_indev_t *indev = lv_indev_get_act();
  if (!indev) return;
  lv_point_t p;
  lv_indev_get_point(indev, &p);
  if (lv_event_get_code(e) == LV_EVENT_PRESSED) {
    sPressY = p.y;
  } else if (lv_event_get_code(e) == LV_EVENT_PRESSING) {
    if (p.y - sPressY > 40) menuOpen();
  }
}

static void scrimClickCb(lv_event_t *e) { menuClose(); }
static void closeClickCb(lv_event_t *e) { menuClose(); }

static void sliderEventCb(lv_event_t *e) {
  lv_obj_t *slider = lv_event_get_target(e);
  int32_t v = lv_slider_get_value(slider);
  displaySetBrightness((uint8_t)v);
  char buf[8];
  snprintf(buf, sizeof(buf), "%d%%", (int)((v * 100 + 127) / 255));
  uiSetLabelText(sValueLabel, buf);
}

static void create(lv_obj_t *screen) {
  const lv_color_t panelBg = lv_color_make(0x20, 0x20, 0x24);

  // Backdrop: dims + click-blocks the dash while the menu is open.
  sScrim = lv_obj_create(screen);
  lv_obj_remove_style_all(sScrim);
  lv_obj_set_size(sScrim, LV_PCT(100), LV_PCT(100));
  lv_obj_set_style_bg_color(sScrim, lv_color_black(), 0);
  lv_obj_set_style_bg_opa(sScrim, LV_OPA_50, 0);
  lv_obj_add_flag(sScrim, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_HIDDEN);
  // No SCROLL_CHAIN: a horizontal drag on the open menu must not swipe the
  // tileview to the race page underneath.
  lv_obj_clear_flag(sScrim,
                    (lv_obj_flag_t)(LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_SCROLL_CHAIN));
  lv_obj_add_event_cb(sScrim, scrimClickCb, LV_EVENT_CLICKED, nullptr);

  // Panel: dark rounded card, flex column so more rows slot in later.
  sPanel = lv_obj_create(screen);
  lv_obj_set_size(sPanel, 340, 290);
  lv_obj_align(sPanel, LV_ALIGN_CENTER, 0, -10);
  lv_obj_set_style_bg_color(sPanel, panelBg, 0);
  lv_obj_set_style_bg_opa(sPanel, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(sPanel, 1, 0);
  lv_obj_set_style_border_color(sPanel, COL_DIM, 0);
  lv_obj_set_style_radius(sPanel, 24, 0);
  lv_obj_set_style_pad_all(sPanel, 20, 0);
  lv_obj_set_style_pad_row(sPanel, 18, 0);
  lv_obj_set_flex_flow(sPanel, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(sPanel, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_add_flag(sPanel, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(sPanel, LV_OBJ_FLAG_SCROLLABLE);

  // Row: "BRIGHTNESS" caption + live percent value.
  lv_obj_t *caption = lv_label_create(sPanel);
  lv_obj_set_style_text_font(caption, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(caption, COL_TEXT, 0);
  lv_label_set_text(caption, "BRIGHTNESS");

  uint8_t cur = displayGetBrightness();
  sValueLabel = lv_label_create(sPanel);
  lv_obj_set_style_text_font(sValueLabel, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(sValueLabel, COL_DIM, 0);
  char buf[8];
  snprintf(buf, sizeof(buf), "%d%%", (int)((cur * 100 + 127) / 255));
  lv_label_set_text(sValueLabel, buf);

  // Slider: 5..255 so the panel can never be turned fully dark.
  lv_obj_t *slider = lv_slider_create(sPanel);
  lv_obj_set_size(slider, 260, 20);
  lv_slider_set_range(slider, 5, 255);
  lv_slider_set_value(slider, cur < 5 ? 5 : cur, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(slider, lv_color_make(0x40, 0x40, 0x48),
                            LV_PART_MAIN);
  lv_obj_set_style_bg_color(slider, COL_TEXT, LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(slider, COL_TEXT, LV_PART_KNOB);
  lv_obj_add_event_cb(slider, sliderEventCb, LV_EVENT_VALUE_CHANGED, nullptr);

  // Close button.
  lv_obj_t *closeBtn = lv_btn_create(sPanel);
  lv_obj_set_size(closeBtn, 140, 56);
  lv_obj_set_style_bg_color(closeBtn, lv_color_make(0x40, 0x40, 0x48), 0);
  lv_obj_set_style_radius(closeBtn, 28, 0);
  lv_obj_add_event_cb(closeBtn, closeClickCb, LV_EVENT_CLICKED, nullptr);
  lv_obj_t *closeLabel = lv_label_create(closeBtn);
  lv_obj_set_style_text_font(closeLabel, &lv_font_montserrat_24, 0);
  lv_obj_set_style_text_color(closeLabel, COL_TEXT, 0);
  lv_label_set_text(closeLabel, LV_SYMBOL_UP " CLOSE");
  lv_obj_center(closeLabel);

  // Top-edge drag zone (invisible). Sized to the visible chord of the round
  // display's top area; created last so it stays above the other plugins.
  sZone = lv_obj_create(screen);
  lv_obj_remove_style_all(sZone);
  lv_obj_set_size(sZone, 240, 70);
  lv_obj_align(sZone, LV_ALIGN_TOP_MID, 0, 0);
  lv_obj_add_flag(sZone, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(sZone, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(sZone, zoneEventCb, LV_EVENT_PRESSED, nullptr);
  lv_obj_add_event_cb(sZone, zoneEventCb, LV_EVENT_PRESSING, nullptr);
}

// The menu is purely event-driven; nothing to map from the DashModel.
static void update(const DashModel &m) {}

extern const UiPlugin kPluginMenu = {"menu", create, update};
