#include "survey.h"
#include "comm.h"
#include "data.h"
#include "reminder.h"
#include "ui.h"

#define SYNC_TIMEOUT_MS 6000
#define CLOSE_DELAY_MS 1800
#define REPEAT_INTERVAL_MS 200

static Window *s_window;
static Layer *s_layer;
static int32_t s_date;
static int s_index;
static uint8_t s_values[MAX_QUESTIONS];

static Window *s_done_window;
static Layer *s_done_layer;
static AppTimer *s_done_timer;
static bool s_synced;
static bool s_sync_gave_up;

// ---------------------------------------------------------------- saved screen

static void done_close(void *context) {
  s_done_timer = NULL;
  window_stack_pop_all(true);
}

static void done_set_timer(uint32_t ms, AppTimerCallback callback) {
  if (s_done_timer) {
    app_timer_cancel(s_done_timer);
  }
  s_done_timer = app_timer_register(ms, callback, NULL);
}

static void done_sync_timeout(void *context) {
  s_sync_gave_up = true;
  layer_mark_dirty(s_done_layer);
  s_done_timer = app_timer_register(CLOSE_DELAY_MS + 1000, done_close, NULL);
}

static void done_synced(void) {
  if (!s_done_window || data_pending_count() > 0) {
    return;
  }
  s_synced = true;
  layer_mark_dirty(s_done_layer);
  done_set_timer(CLOSE_DELAY_MS, done_close);
}

static void done_layer_update(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  char date[16];
  data_format_date(s_date, date, sizeof(date));
  int y = ui_draw_header(ctx, bounds, date, ui_section_color(SECTION_DAILY));
  y += (bounds.size.h - y) / 2 - PBL_IF_ROUND_ELSE(48, 44);
  y = ui_draw_text(ctx, "Saved", FONT_KEY_GOTHIC_28_BOLD, y, bounds);
  const char *status = s_synced ? "Synced to phone"
                     : s_sync_gave_up ? "Kept on watch. Will sync when the phone is connected."
                     : "Syncing...";
  ui_draw_text(ctx, status, FONT_KEY_GOTHIC_18, y, bounds);
}

static void done_click(ClickRecognizerRef recognizer, void *context) {
  window_stack_pop_all(true);
}

static void done_click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, done_click);
  window_single_click_subscribe(BUTTON_ID_BACK, done_click);
}

static void done_window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  s_done_layer = layer_create(layer_get_bounds(root));
  layer_set_update_proc(s_done_layer, done_layer_update);
  layer_add_child(root, s_done_layer);
}

static void done_window_unload(Window *window) {
  if (s_done_timer) {
    app_timer_cancel(s_done_timer);
    s_done_timer = NULL;
  }
  comm_set_synced_handler(NULL);
  layer_destroy(s_done_layer);
  window_destroy(s_done_window);
  s_done_window = NULL;
}

static void done_window_push(void) {
  s_synced = false;
  s_sync_gave_up = false;
  s_done_window = window_create();
  window_set_click_config_provider(s_done_window, done_click_config_provider);
  window_set_window_handlers(s_done_window, (WindowHandlers) {
    .load = done_window_load,
    .unload = done_window_unload,
  });
  window_stack_push(s_done_window, true);
  comm_set_synced_handler(done_synced);
  done_set_timer(SYNC_TIMEOUT_MS, done_sync_timeout);
}

// ------------------------------------------------------------------- questions

static void load_values(void) {
  s_index = 0;
  for (int i = 0; i < data_question_count(); i++) {
    s_values[i] = data_initial_value(data_question(i));
  }
}

static void finish(void) {
  Entry entry;
  memset(&entry, 0, sizeof(entry));
  entry.date = s_date;
  entry.count = data_question_count();
  for (int i = 0; i < entry.count; i++) {
    entry.answers[i] = (Answer) { .id = data_question(i)->id, .value = s_values[i] };
  }
  data_save_entry(&entry);
  reminder_schedule_daily();

  Window *survey = s_window;
  done_window_push();
  window_stack_remove(survey, false);
  comm_flush();
}

static void change_value(int delta) {
  const Question *question = data_question(s_index);
  int max = question->type == TYPE_SCALE ? SCALE_MAX : 1;
  int value = s_values[s_index];
  if (question->type == TYPE_BOOL) {
    value = !value;
  } else {
    value += delta;
    if (value < 0) value = 0;
    if (value > max) value = max;
  }
  s_values[s_index] = value;
  layer_mark_dirty(s_layer);
}

static void up_click(ClickRecognizerRef recognizer, void *context) {
  change_value(1);
}

static void down_click(ClickRecognizerRef recognizer, void *context) {
  change_value(-1);
}

static void select_click(ClickRecognizerRef recognizer, void *context) {
  if (s_index + 1 < data_question_count()) {
    s_index++;
    layer_mark_dirty(s_layer);
  } else {
    finish();
  }
}

static void back_click(ClickRecognizerRef recognizer, void *context) {
  if (s_index > 0) {
    s_index--;
    layer_mark_dirty(s_layer);
  } else {
    window_stack_pop(true);
  }
}

static void click_config_provider(void *context) {
  window_single_repeating_click_subscribe(BUTTON_ID_UP, REPEAT_INTERVAL_MS, up_click);
  window_single_repeating_click_subscribe(BUTTON_ID_DOWN, REPEAT_INTERVAL_MS, down_click);
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click);
  window_single_click_subscribe(BUTTON_ID_BACK, back_click);
}

static void draw_value(GContext *ctx, GRect box, const Question *question, uint8_t value) {
  GFont value_font = fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD);
  GFont suffix_font = fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD);
  char text[8];
  const char *suffix = "";
  if (question->type == TYPE_BOOL) {
    strncpy(text, value ? "Yes" : "No", sizeof(text));
  } else {
    snprintf(text, sizeof(text), (value % 2) ? "%d.5" : "%d", value / 2);
    suffix = "/5";
  }

  GRect measure = GRect(0, 0, 300, 60);
  int value_width = graphics_text_layout_get_content_size(
      text, value_font, measure, GTextOverflowModeFill, GTextAlignmentLeft).w;
  int suffix_width = suffix[0] ? 4 + graphics_text_layout_get_content_size(
      suffix, suffix_font, measure, GTextOverflowModeFill, GTextAlignmentLeft).w : 0;
  int x = box.origin.x + (box.size.w - value_width - suffix_width) / 2;

  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, text, value_font, GRect(x, box.origin.y, value_width + 4, 50),
                     GTextOverflowModeFill, GTextAlignmentLeft, NULL);
  if (suffix[0]) {
    graphics_draw_text(ctx, suffix, suffix_font,
                       GRect(x + value_width + 4, box.origin.y + 15, suffix_width + 4, 30),
                       GTextOverflowModeFill, GTextAlignmentLeft, NULL);
  }
}

static void layer_update(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  int count = data_question_count();
  const Question *question = data_question(s_index);
  uint8_t value = s_values[s_index];

  // Section title with a progress bar underneath.
  GColor color = ui_section_color(question->section);
  int y = ui_draw_header(ctx, bounds, data_section_title(question->section), color);
  graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(GColorLightGray, GColorWhite));
  graphics_fill_rect(ctx, GRect(0, y, bounds.size.w, 4), 0, GCornerNone);
  graphics_context_set_fill_color(ctx, PBL_IF_COLOR_ELSE(color, GColorBlack));
  graphics_fill_rect(ctx, GRect(0, y + 1, bounds.size.w * (s_index + 1) / count, 3), 0, GCornerNone);
  y += 5;

  // Value at the bottom, question text centred in the space that is left.
  int value_height = 50;
  int value_y = bounds.size.h - value_height - PBL_IF_ROUND_ELSE(18, 4);
  int button_column = PBL_IF_ROUND_ELSE(0, 14);
  GRect text_box = GRect(UI_MARGIN, y, bounds.size.w - 2 * UI_MARGIN - button_column, value_y - y);

  static const char *const font_keys[] = {
    FONT_KEY_GOTHIC_24_BOLD, FONT_KEY_GOTHIC_18_BOLD, FONT_KEY_GOTHIC_14_BOLD,
  };
  GFont font = NULL;
  int text_height = 0;
  for (unsigned i = 0; i < ARRAY_LENGTH(font_keys); i++) {
    font = fonts_get_system_font(font_keys[i]);
    text_height = ui_text_height(question->label, font, text_box.size.w);
    if (text_height <= text_box.size.h) {
      break;
    }
  }
  if (text_height < text_box.size.h) {
    text_box.origin.y += (text_box.size.h - text_height) / 2 - 3;
  }
  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, question->label, font, text_box, GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentCenter, NULL);

  draw_value(ctx, GRect(UI_MARGIN, value_y, bounds.size.w - 2 * UI_MARGIN - button_column, value_height),
             question, value);

  // Button hints: hide an arrow when the value cannot go further that way.
  int max = question->type == TYPE_SCALE ? SCALE_MAX : 1;
  if (question->type == TYPE_BOOL || value < max) {
    // A tall section title can reach the up button; keep the arrow below it.
    GPoint up = ui_button_point(bounds, BUTTON_ID_UP);
    if (up.y < y + 6) {
      up.y = y + 6;
    }
    ui_draw_arrow(ctx, up, ARROW_UP);
  }
  if (question->type == TYPE_BOOL || value > 0) {
    ui_draw_arrow(ctx, ui_button_point(bounds, BUTTON_ID_DOWN), ARROW_DOWN);
  }
  ui_draw_arrow(ctx, ui_button_point(bounds, BUTTON_ID_SELECT), ARROW_RIGHT);
}

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  s_layer = layer_create(layer_get_bounds(root));
  layer_set_update_proc(s_layer, layer_update);
  layer_add_child(root, s_layer);
}

static void window_unload(Window *window) {
  layer_destroy(s_layer);
  window_destroy(s_window);
  s_window = NULL;
}

bool survey_is_open(void) {
  return s_window != NULL;
}

void survey_reload(void) {
  if (!s_window) {
    return;
  }
  load_values();
  layer_mark_dirty(s_layer);
}

void survey_window_push(int32_t date) {
  if (s_window) {
    return;
  }
  s_date = date;
  load_values();
  s_window = window_create();
  window_set_click_config_provider(s_window, click_config_provider);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);
}
