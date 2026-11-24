#pragma once
#include <pebble.h>

// AppMessage link to the phone: receives settings and questions, and uploads
// completed entries until the phone acknowledges each one.

typedef void (*CommHandler)(void);

void comm_init(void);
// Tries to upload the oldest queued entry. Safe to call at any time.
void comm_flush(void);
// Called whenever a queued entry has been acknowledged by the phone.
void comm_set_synced_handler(CommHandler handler);
// Called after new settings or questions arrive from the phone.
void comm_set_config_handler(CommHandler handler);
