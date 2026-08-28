// UI plugin interface: each screen element is a self-contained module that
// builds its own LVGL objects (create) and maps the latest DashModel onto
// them (update). Plugins never touch BLE or FreeRTOS — the render task calls
// update() with a model the data hub produced.
#pragma once

#include <lvgl.h>
#include <string.h>

#include "dash_model.h"

struct UiPlugin {
  const char *name;
  void (*create)(lv_obj_t *screen);
  void (*update)(const DashModel &m);
};

// Sets a label's text only when it changed — every set invalidates the
// label's area, so unconditional sets redraw the screen for no reason.
static inline void uiSetLabelText(lv_obj_t *label, const char *text) {
  if (strcmp(lv_label_get_text(label), text) != 0) {
    lv_label_set_text(label, text);
  }
}
