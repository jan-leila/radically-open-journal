#pragma once
#include <pebble.h>

// Small drawing helpers shared by the windows.

// Horizontal margin that keeps text clear of the bezel on round watches.
#define UI_MARGIN PBL_IF_ROUND_ELSE(28, 4)

typedef enum { ARROW_UP, ARROW_DOWN, ARROW_RIGHT } ArrowDirection;

GColor ui_section_color(uint8_t section);

// Draws a title band across the top and returns the y just below it.
int ui_draw_header(GContext *ctx, GRect bounds, const char *title, GColor background);

int ui_text_height(const char *text, GFont font, int width);

// Draws wrapped, centred text and returns the y just below it.
int ui_draw_text(GContext *ctx, const char *text, const char *font_key, int y, GRect bounds);

void ui_draw_arrow(GContext *ctx, GPoint center, ArrowDirection direction);

// Where the up / select / down buttons sit, as points near the right edge.
GPoint ui_button_point(GRect bounds, ButtonId button);
