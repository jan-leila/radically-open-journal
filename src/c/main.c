#include <pebble.h>
#include "comm.h"
#include "data.h"
#include "reminder.h"
#include "survey.h"
#include "ui.h"

// Home screen: shows whether a day's card is filled in; SELECT opens it.
// UP steps back to earlier days so a missed card can still be filled in.

static Window *s_window;
static Layer *s_layer;
static int s_days_back;

static int32_t selected_day(void) {
  return data_date_offset(data_diary_day(time(NULL)), -s_days_back);
}

static void layer_update(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  int32_t day = selected_day();
  bool filled = data_is_filled(day);
  // Only the latest day's answers are kept on the watch, so an older day that
  // is opened again starts over from those rather than from its own answers.
  bool editable = day == data_last_entry()->date;

  char date[16];
  data_format_date(day, date, sizeof(date));

  char footer[32];
  int pending = data_pending_count();
  if (s_days_back == 1) {
    strncpy(footer, "Yesterday", sizeof(footer));
  } else if (s_days_back > 1) {
    snprintf(footer, sizeof(footer), "%d days ago", s_days_back);
  } else if (pending > 0) {
    snprintf(footer, sizeof(footer), "%d waiting to sync", pending);
  } else {
    strncpy(footer, "UP for earlier days", sizeof(footer));
  }

  const char *action = !filled ? "SELECT to start" : editable ? "SELECT to edit" : "SELECT to redo";

  int y = ui_draw_header(ctx, bounds, "RO Diary Card", ui_section_color(SECTION_DAILY));
  y += PBL_IF_ROUND_ELSE(0, 2) + (bounds.size.h - 168) / 4;
  y = ui_draw_text(ctx, date, FONT_KEY_GOTHIC_24_BOLD, y, bounds);
  y = ui_draw_text(ctx, filled ? "Filled in" : "Not filled in yet", FONT_KEY_GOTHIC_18, y, bounds);
  y = ui_draw_text(ctx, action, FONT_KEY_GOTHIC_18_BOLD, y + 4, bounds);
  ui_draw_text(ctx, footer, FONT_KEY_GOTHIC_14, bounds.size.h - PBL_IF_ROUND_ELSE(44, 22), bounds);

  ui_draw_arrow(ctx, ui_button_point(bounds, BUTTON_ID_SELECT), ARROW_RIGHT);
  if (s_days_back < MAX_BACKFILL_DAYS) {
    ui_draw_arrow(ctx, ui_button_point(bounds, BUTTON_ID_UP), ARROW_UP);
  }
  if (s_days_back > 0) {
    ui_draw_arrow(ctx, ui_button_point(bounds, BUTTON_ID_DOWN), ARROW_DOWN);
  }
}

static void select_click(ClickRecognizerRef recognizer, void *context) {
  survey_window_push(selected_day());
}

static void earlier_click(ClickRecognizerRef recognizer, void *context) {
  if (s_days_back < MAX_BACKFILL_DAYS) {
    s_days_back++;
    layer_mark_dirty(s_layer);
  }
}

static void later_click(ClickRecognizerRef recognizer, void *context) {
  if (s_days_back > 0) {
    s_days_back--;
    layer_mark_dirty(s_layer);
  }
}

static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click);
  window_single_repeating_click_subscribe(BUTTON_ID_UP, 150, earlier_click);
  window_single_repeating_click_subscribe(BUTTON_ID_DOWN, 150, later_click);
}

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  s_layer = layer_create(layer_get_bounds(root));
  layer_set_update_proc(s_layer, layer_update);
  layer_add_child(root, s_layer);
}

static void window_unload(Window *window) {
  layer_destroy(s_layer);
  s_layer = NULL;
}

static void config_changed(void) {
  reminder_schedule_daily();
  survey_reload();
  if (s_layer) {
    layer_mark_dirty(s_layer);
  }
}

static void pending_changed(void) {
  if (s_layer) {
    layer_mark_dirty(s_layer);
  }
}

// A reminder that fires while the app is already open.
static void wakeup_handler(WakeupId id, int32_t cookie) {
  if (survey_is_open()) {
    reminder_schedule_snooze();
  } else if (data_last_entry()->date == data_diary_day(time(NULL))) {
    reminder_schedule_daily();
  } else {
    reminder_window_push();
  }
}

static void init(void) {
  data_init();
  comm_set_config_handler(config_changed);
  comm_set_synced_handler(pending_changed);
  comm_init();
  wakeup_service_subscribe(wakeup_handler);

  bool card_done = data_last_entry()->date == data_diary_day(time(NULL));
  if (launch_reason() == APP_LAUNCH_WAKEUP && !card_done) {
    reminder_window_push();
    return;
  }

  s_window = window_create();
  window_set_click_config_provider(s_window, click_config_provider);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);
}

static void deinit(void) {
  reminder_ensure_scheduled();
  if (s_window) {
    window_destroy(s_window);
  }
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
