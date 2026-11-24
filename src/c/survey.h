#pragma once
#include <pebble.h>

// The diary card itself: one question per screen, then a "saved" screen.

// Opens the card for the given diary day (yyyymmdd).
void survey_window_push(int32_t date);
bool survey_is_open(void);
// Restarts an open survey after the question list changed underneath it.
void survey_reload(void);
