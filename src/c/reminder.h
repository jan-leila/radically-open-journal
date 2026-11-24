#pragma once
#include <pebble.h>

// Daily reminder, built on the Wakeup API so it works without the phone.

#define WAKEUP_COOKIE_DAILY 0
#define WAKEUP_COOKIE_SNOOZE 1

// Schedules the next daily reminder, skipping a day that is already filled in.
void reminder_schedule_daily(void);
// Schedules a reminder after the snooze interval.
void reminder_schedule_snooze(void);
// Makes sure a reminder is pending (or none, if reminders are off).
void reminder_ensure_scheduled(void);
// Shows the reminder screen with its snooze / start / ignore choices.
void reminder_window_push(void);
