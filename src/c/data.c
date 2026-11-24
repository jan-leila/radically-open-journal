#include "data.h"

static Settings s_settings;
static Question s_questions[MAX_QUESTIONS];
static int s_question_count;
static Entry s_last_entry;

static const Question FIXED_QUESTIONS[NUM_FIXED] = {
  { .id = 1, .type = TYPE_SCALE, .section = SECTION_DAILY, .label = "Urge to commit suicide" },
  { .id = 2, .type = TYPE_BOOL, .section = SECTION_DAILY, .label = "Taken prescribed meds" },
  { .id = 3, .type = TYPE_BOOL, .section = SECTION_DAILY, .label = "Other drugs & alcohol" },
};

static void load_questions(void) {
  memcpy(s_questions, FIXED_QUESTIONS, sizeof(FIXED_QUESTIONS));
  s_question_count = NUM_FIXED;

  int custom = persist_exists(KEY_CUSTOM_COUNT) ? persist_read_int(KEY_CUSTOM_COUNT) : 0;
  if (custom > MAX_CUSTOM) {
    custom = MAX_CUSTOM;
  }
  for (int i = 0; i < custom; i++) {
    Question *q = &s_questions[s_question_count];
    if (persist_read_data(KEY_QUESTION_BASE + i, q, sizeof(Question)) != (int)sizeof(Question)) {
      continue;
    }
    q->label[LABEL_LEN - 1] = '\0';
    s_question_count++;
  }
}

void data_init(void) {
  s_settings = (Settings) { .reminder_enabled = 1, .hour = 21, .minute = 0, .snooze_min = 15 };
  if (persist_exists(KEY_SETTINGS)) {
    persist_read_data(KEY_SETTINGS, &s_settings, sizeof(s_settings));
  }

  memset(&s_last_entry, 0, sizeof(s_last_entry));
  if (persist_exists(KEY_LAST_ENTRY)) {
    persist_read_data(KEY_LAST_ENTRY, &s_last_entry, sizeof(s_last_entry));
    if (s_last_entry.count > MAX_QUESTIONS) {
      s_last_entry.count = MAX_QUESTIONS;
    }
  }

  load_questions();
}

const Settings *data_settings(void) {
  return &s_settings;
}

void data_set_settings(const Settings *settings) {
  s_settings = *settings;
  if (s_settings.hour > 23) s_settings.hour = 21;
  if (s_settings.minute > 59) s_settings.minute = 0;
  if (s_settings.snooze_min < 1) s_settings.snooze_min = 15;
  persist_write_data(KEY_SETTINGS, &s_settings, sizeof(s_settings));
}

int data_question_count(void) {
  return s_question_count;
}

const Question *data_question(int index) {
  return &s_questions[index];
}

const char *data_section_title(uint8_t section) {
  switch (section) {
    // On round watches the top of the screen is narrow, so the first line is kept short.
    case SECTION_TSE: return PBL_IF_ROUND_ELSE("Thoughts,\nSensations, Emotions",
                                               "Thoughts, Sensations, Emotions");
    case SECTION_SOCIAL: return "Social Signaling";
    default: return "Daily";
  }
}

void data_stage_custom_question(int index, const Question *question) {
  if (index < 0 || index >= MAX_CUSTOM) {
    return;
  }
  persist_write_data(KEY_QUESTION_BASE + index, question, sizeof(Question));
}

void data_commit_custom_questions(int count) {
  if (count < 0) count = 0;
  if (count > MAX_CUSTOM) count = MAX_CUSTOM;
  persist_write_int(KEY_CUSTOM_COUNT, count);
  for (int i = count; i < MAX_CUSTOM; i++) {
    persist_delete(KEY_QUESTION_BASE + i);
  }
  load_questions();
}

const Entry *data_last_entry(void) {
  return &s_last_entry;
}

// Days since 1970-01-01 for a proleptic Gregorian date.
static int32_t days_from_civil(int y, int m, int d) {
  y -= m <= 2;
  int era = (y >= 0 ? y : y - 399) / 400;
  int yoe = y - era * 400;
  int doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + doe - 719468;
}

static int32_t day_number(int32_t date) {
  return days_from_civil(date / 10000, (date / 100) % 100, date % 100);
}

uint8_t data_initial_value(const Question *question) {
  uint8_t fallback = question->type == TYPE_SCALE ? SCALE_DEFAULT : 0;
  for (int i = 0; i < s_last_entry.count; i++) {
    if (s_last_entry.answers[i].id == question->id) {
      uint8_t value = s_last_entry.answers[i].value;
      uint8_t max = question->type == TYPE_SCALE ? SCALE_MAX : 1;
      return value > max ? max : value;
    }
  }
  return fallback;
}

// Which recent days have a card: bit n is the day n days before `anchor`.
typedef struct __attribute__((packed)) {
  int32_t anchor;
  uint32_t bits;
} FilledDays;

static void mark_filled(int32_t date) {
  FilledDays filled = { .anchor = 0, .bits = 0 };
  int32_t day = day_number(date);
  if (persist_read_data(KEY_FILLED_DAYS, &filled, sizeof(filled)) != (int)sizeof(filled)) {
    filled = (FilledDays) { .anchor = day, .bits = 0 };
  }
  if (day > filled.anchor) {
    int32_t shift = day - filled.anchor;
    filled.bits = shift >= 32 ? 0 : filled.bits << shift;
    filled.anchor = day;
  }
  int32_t age = filled.anchor - day;
  if (age < 32) {
    filled.bits |= 1u << age;
  }
  persist_write_data(KEY_FILLED_DAYS, &filled, sizeof(filled));
}

bool data_is_filled(int32_t date) {
  if (date == s_last_entry.date) {
    return true;
  }
  FilledDays filled;
  if (persist_read_data(KEY_FILLED_DAYS, &filled, sizeof(filled)) != (int)sizeof(filled)) {
    return false;
  }
  int32_t age = filled.anchor - day_number(date);
  return age >= 0 && age < 32 && (filled.bits & (1u << age));
}

int32_t data_date_offset(int32_t date, int days) {
  // Inverse of days_from_civil.
  int32_t z = day_number(date) + days + 719468;
  int32_t era = (z >= 0 ? z : z - 146096) / 146097;
  int32_t doe = z - era * 146097;
  int32_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  int32_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  int32_t mp = (5 * doy + 2) / 153;
  int32_t d = doy - (153 * mp + 2) / 5 + 1;
  int32_t m = mp < 10 ? mp + 3 : mp - 9;
  int32_t y = yoe + era * 400 + (m <= 2);
  return y * 10000 + m * 100 + d;
}

static bool read_pending(int slot, Entry *out) {
  int key = KEY_PENDING_BASE + slot;
  return persist_exists(key) &&
         persist_read_data(key, out, sizeof(Entry)) == (int)sizeof(Entry) &&
         out->date != 0;
}

void data_save_entry(const Entry *entry) {
  // Answers are pre-filled from the latest day, so back-filling an older day
  // must not replace it.
  if (entry->date >= s_last_entry.date) {
    s_last_entry = *entry;
    persist_write_data(KEY_LAST_ENTRY, &s_last_entry, sizeof(s_last_entry));
  }
  mark_filled(entry->date);

  // Queue for the phone: reuse this day's slot, else a free one, else the oldest.
  int target = -1;
  int free_slot = -1;
  int oldest_slot = 0;
  int32_t oldest_date = INT32_MAX;
  Entry existing;
  for (int i = 0; i < MAX_PENDING; i++) {
    if (!read_pending(i, &existing)) {
      if (free_slot < 0) free_slot = i;
      continue;
    }
    if (existing.date == entry->date) {
      target = i;
      break;
    }
    if (existing.date < oldest_date) {
      oldest_date = existing.date;
      oldest_slot = i;
    }
  }
  if (target < 0) {
    target = free_slot >= 0 ? free_slot : oldest_slot;
  }
  persist_write_data(KEY_PENDING_BASE + target, entry, sizeof(Entry));
}

int data_pending_count(void) {
  int count = 0;
  Entry entry;
  for (int i = 0; i < MAX_PENDING; i++) {
    if (read_pending(i, &entry)) count++;
  }
  return count;
}

bool data_pending_oldest(Entry *out) {
  bool found = false;
  Entry entry;
  for (int i = 0; i < MAX_PENDING; i++) {
    if (read_pending(i, &entry) && (!found || entry.date < out->date)) {
      *out = entry;
      found = true;
    }
  }
  return found;
}

void data_pending_remove(int32_t date) {
  Entry entry;
  for (int i = 0; i < MAX_PENDING; i++) {
    if (read_pending(i, &entry) && entry.date == date) {
      persist_delete(KEY_PENDING_BASE + i);
    }
  }
}

int32_t data_diary_day(time_t when) {
  when -= DAY_ROLLOVER_HOUR * SECONDS_PER_HOUR;
  struct tm *tm = localtime(&when);
  return (tm->tm_year + 1900) * 10000 + (tm->tm_mon + 1) * 100 + tm->tm_mday;
}

void data_format_date(int32_t date, char *buffer, size_t size) {
  struct tm tm = {
    .tm_year = date / 10000 - 1900,
    .tm_mon = (date / 100) % 100 - 1,
    .tm_mday = date % 100,
  };
  // 1970-01-01 was a Thursday.
  tm.tm_wday = (int)(((day_number(date) % 7) + 7 + 4) % 7);
  strftime(buffer, size, "%a %b %e", &tm);
}
