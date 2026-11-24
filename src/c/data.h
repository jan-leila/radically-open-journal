#pragma once
#include <pebble.h>

// Storage and date helpers shared by the whole watch app.
//
// The watch only keeps what it needs to run the survey offline: the question
// list, the most recent entry (to pre-fill answers) and a small queue of
// entries the phone has not acknowledged yet. The full history lives on the
// phone.

#define NUM_FIXED 3
#define MAX_CUSTOM 24
#define MAX_QUESTIONS (NUM_FIXED + MAX_CUSTOM)
#define LABEL_LEN 64
#define MAX_PENDING 10

// Scale answers are stored in half points: 0..10 means 0..5.
#define SCALE_MAX 10
#define SCALE_DEFAULT 5

// Entries made before this hour count for the previous day, so a card that is
// filled in (or snoozed until) just after midnight still lands on the right day.
#define DAY_ROLLOVER_HOUR 4

// How many days back from today a card can still be filled in on the watch.
#define MAX_BACKFILL_DAYS 30

// Persistent storage keys.
#define KEY_SETTINGS 1
#define KEY_CUSTOM_COUNT 2
#define KEY_LAST_ENTRY 3
#define KEY_WAKEUP_ID 4
#define KEY_AUTO_SNOOZES 5
#define KEY_FILLED_DAYS 6
#define KEY_QUESTION_BASE 100
#define KEY_PENDING_BASE 200

typedef enum { TYPE_SCALE = 0, TYPE_BOOL = 1 } QuestionType;
typedef enum { SECTION_DAILY = 0, SECTION_TSE = 1, SECTION_SOCIAL = 2 } QuestionSection;

typedef struct __attribute__((packed)) {
  uint16_t id;
  uint8_t type;
  uint8_t section;
  char label[LABEL_LEN];
} Question;

typedef struct __attribute__((packed)) {
  uint16_t id;
  uint8_t value;
} Answer;

typedef struct __attribute__((packed)) {
  int32_t date;  // yyyymmdd, 0 when empty
  uint8_t count;
  Answer answers[MAX_QUESTIONS];
} Entry;

typedef struct __attribute__((packed)) {
  uint8_t reminder_enabled;
  uint8_t hour;
  uint8_t minute;
  uint8_t snooze_min;
} Settings;

void data_init(void);

const Settings *data_settings(void);
void data_set_settings(const Settings *settings);

int data_question_count(void);
const Question *data_question(int index);
const char *data_section_title(uint8_t section);

// Config sync: slots are staged in persistent storage, then committed at once.
void data_stage_custom_question(int index, const Question *question);
void data_commit_custom_questions(int count);

// The entry with the latest date (not necessarily the one saved most recently).
const Entry *data_last_entry(void);
// Starting value for a question: the answer from the latest day entered.
uint8_t data_initial_value(const Question *question);
void data_save_entry(const Entry *entry);

int data_pending_count(void);
bool data_pending_oldest(Entry *out);
void data_pending_remove(int32_t date);

// The diary day (yyyymmdd) that a moment in time belongs to.
int32_t data_diary_day(time_t when);
// The date a number of days before (negative) or after `date`.
int32_t data_date_offset(int32_t date, int days);
// Whether the watch has recorded a card for this date (recent days only).
bool data_is_filled(int32_t date);
// Writes e.g. "Mon Nov 23" for a yyyymmdd date.
void data_format_date(int32_t date, char *buffer, size_t size);
