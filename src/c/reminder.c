#include "reminder.h"
#include "data.h"
#include "survey.h"
#include "ui.h"

#define MAX_AUTO_SNOOZES 3
#define UNANSWERED_TIMEOUT_MS (2 * 60 * 1000)

static Window *s_window;
static Layer *s_layer;
static AppTimer *s_timeout;
static char s_snooze_label[20];

static void schedule(time_t when, int32_t cookie) {
  wakeup_cancel_all();
  WakeupId id = -1;
  // Wakeups from different apps may not share a minute, so nudge until one fits.
  for (int attempt = 0; attempt < 15; attempt++) {
    id = wakeup_schedule(when + attempt * SECONDS_PER_MINUTE, cookie,
                         cookie == WAKEUP_COOKIE_DAILY);
    if (id != E_RANGE) {
      break;
    }
  }
  persist_write_int(KEY_WAKEUP_ID, id);
}

static time_t at_reminder_time(time_t day) {
  const Settings *settings = data_settings();
  struct tm tm = *localtime(&day);
  tm.tm_hour = settings->hour;
  tm.tm_min = settings->minute;
  tm.tm_sec = 0;
  return mktime(&tm);
}

void reminder_schedule_daily(void) {
  persist_write_int(KEY_AUTO_SNOOZES, 0);
  if (!data_settings()->reminder_enabled) {
    wakeup_cancel_all();
    persist_delete(KEY_WAKEUP_ID);
    return;
  }
  time_t now = time(NULL);
  time_t when = at_reminder_time(now);
  for (int i = 0; i < 3; i++) {
    bool already_done = data_diary_day(when) == data_last_entry()->date;
    if (when > now + 30 && !already_done) {
      break;
    }
    when = at_reminder_time(when + SECONDS_PER_DAY);
  }
  schedule(when, WAKEUP_COOKIE_DAILY);
}

void reminder_schedule_snooze(void) {
  time_t now = time(NULL);
  time_t when = now + data_settings()->snooze_min * SECONDS_PER_MINUTE;
  if (!data_settings()->reminder_enabled || data_diary_day(when) != data_diary_day(now)) {
    // Snoozing past the end of the diary day: fall back to the normal reminder.
    reminder_schedule_daily();
    return;
  }
  schedule(when, WAKEUP_COOKIE_SNOOZE);
}

void reminder_ensure_scheduled(void) {
  time_t when = 0;
  bool pending = data_settings()->reminder_enabled && persist_exists(KEY_WAKEUP_ID) &&
                 wakeup_query(persist_read_int(KEY_WAKEUP_ID), &when) && when > time(NULL);
  if (!pending) {
    reminder_schedule_daily();
  }
}

static void cancel_timeout(void) {
  if (s_timeout) {
    app_timer_cancel(s_timeout);
    s_timeout = NULL;
  }
}

static void snooze_click(ClickRecognizerRef recognizer, void *context) {
  persist_write_int(KEY_AUTO_SNOOZES, 0);
  reminder_schedule_snooze();
  window_stack_pop_all(true);
}

static void ignore_click(ClickRecognizerRef recognizer, void *context) {
  reminder_schedule_daily();
  window_stack_pop_all(true);
}

static void start_click(ClickRecognizerRef recognizer, void *context) {
  // If the card is abandoned half way, remind again after the snooze interval.
  persist_write_int(KEY_AUTO_SNOOZES, 0);
  reminder_schedule_snooze();
  Window *reminder = s_window;
  survey_window_push(data_diary_day(time(NULL)));
  window_stack_remove(reminder, false);
}

static void timeout_elapsed(void *context) {
  s_timeout = NULL;
  // Nobody answered: nudge again a few times, then leave it until tomorrow.
  int auto_snoozes = persist_read_int(KEY_AUTO_SNOOZES);
  if (auto_snoozes < MAX_AUTO_SNOOZES) {
    reminder_schedule_snooze();
    persist_write_int(KEY_AUTO_SNOOZES, auto_snoozes + 1);
  } else {
    reminder_schedule_daily();
  }
  window_stack_pop_all(true);
}

static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_UP, snooze_click);
  window_single_click_subscribe(BUTTON_ID_SELECT, start_click);
  window_single_click_subscribe(BUTTON_ID_DOWN, ignore_click);
  window_single_click_subscribe(BUTTON_ID_BACK, ignore_click);
}

static void draw_choice(GContext *ctx, GRect bounds, ButtonId button, const char *text,
                        const char *font_key) {
  GFont font = fonts_get_system_font(font_key);
  GPoint point = ui_button_point(bounds, button);
  int right = point.x - 10;
  int left = PBL_IF_ROUND_ELSE(20, 2);
  int height = ui_text_height(text, font, right - left);
  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, text, font,
                     GRect(left, point.y - height / 2 - height / 4, right - left, height + 6),
                     GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, NULL);
  ui_draw_arrow(ctx, point, ARROW_RIGHT);
}

static void layer_update(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  ui_draw_header(ctx, bounds, "Diary card time", ui_section_color(SECTION_DAILY));
  draw_choice(ctx, bounds, BUTTON_ID_UP, s_snooze_label, FONT_KEY_GOTHIC_18_BOLD);
  draw_choice(ctx, bounds, BUTTON_ID_SELECT, "Start", FONT_KEY_GOTHIC_28_BOLD);
  draw_choice(ctx, bounds, BUTTON_ID_DOWN, "Ignore", FONT_KEY_GOTHIC_18_BOLD);
}

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  s_layer = layer_create(layer_get_bounds(root));
  layer_set_update_proc(s_layer, layer_update);
  layer_add_child(root, s_layer);
  snprintf(s_snooze_label, sizeof(s_snooze_label), "Snooze %d min", data_settings()->snooze_min);
}

static void window_appear(Window *window) {
  static const uint32_t segments[] = { 400, 200, 400 };
  vibes_enqueue_custom_pattern((VibePattern) {
    .durations = segments,
    .num_segments = ARRAY_LENGTH(segments),
  });
  light_enable_interaction();
  cancel_timeout();
  s_timeout = app_timer_register(UNANSWERED_TIMEOUT_MS, timeout_elapsed, NULL);
}

static void window_disappear(Window *window) {
  cancel_timeout();
}

static void window_unload(Window *window) {
  layer_destroy(s_layer);
  window_destroy(s_window);
  s_window = NULL;
}

void reminder_window_push(void) {
  if (s_window) {
    return;
  }
  s_window = window_create();
  window_set_click_config_provider(s_window, click_config_provider);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .appear = window_appear,
    .disappear = window_disappear,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);
}
