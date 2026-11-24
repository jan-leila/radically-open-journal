#include "ui.h"
#include "data.h"

#define ARROW_SIZE 5

GColor ui_section_color(uint8_t section) {
#if defined(PBL_COLOR)
  switch (section) {
    case SECTION_TSE: return GColorImperialPurple;
    case SECTION_SOCIAL: return GColorDarkGreen;
    default: return GColorCobaltBlue;
  }
#else
  return GColorBlack;
#endif
}

int ui_text_height(const char *text, GFont font, int width) {
  return graphics_text_layout_get_content_size(text, font, GRect(0, 0, width, 500),
                                               GTextOverflowModeWordWrap,
                                               GTextAlignmentCenter).h;
}

int ui_draw_header(GContext *ctx, GRect bounds, const char *title, GColor background) {
  GFont font = fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD);
  int top = PBL_IF_ROUND_ELSE(8, 0);
  int margin = PBL_IF_ROUND_ELSE(26, 2);
  int width = bounds.size.w - 2 * margin;
  int text_height = ui_text_height(title, font, width);
  int bottom = top + text_height + 6;

  graphics_context_set_fill_color(ctx, background);
  graphics_fill_rect(ctx, GRect(0, 0, bounds.size.w, bottom), 0, GCornerNone);
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, title, font, GRect(margin, top - 1, width, text_height + 6),
                     GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
  return bottom;
}

int ui_draw_text(GContext *ctx, const char *text, const char *font_key, int y, GRect bounds) {
  GFont font = fonts_get_system_font(font_key);
  int width = bounds.size.w - 2 * UI_MARGIN;
  int height = ui_text_height(text, font, width);
  graphics_context_set_text_color(ctx, GColorBlack);
  graphics_draw_text(ctx, text, font, GRect(UI_MARGIN, y, width, height + 6),
                     GTextOverflowModeWordWrap, GTextAlignmentCenter, NULL);
  return y + height + 4;
}

void ui_draw_arrow(GContext *ctx, GPoint center, ArrowDirection direction) {
  graphics_context_set_stroke_color(ctx, GColorBlack);
  for (int i = 0; i <= ARROW_SIZE; i++) {
    int offset = i - ARROW_SIZE / 2;
    switch (direction) {
      case ARROW_UP:
        graphics_draw_line(ctx, GPoint(center.x - i, center.y + offset),
                           GPoint(center.x + i, center.y + offset));
        break;
      case ARROW_DOWN:
        graphics_draw_line(ctx, GPoint(center.x - i, center.y - offset),
                           GPoint(center.x + i, center.y - offset));
        break;
      case ARROW_RIGHT:
        graphics_draw_line(ctx, GPoint(center.x - offset, center.y - i),
                           GPoint(center.x - offset, center.y + i));
        break;
    }
  }
}

GPoint ui_button_point(GRect bounds, ButtonId button) {
  int x = bounds.size.w - PBL_IF_ROUND_ELSE(16, 8);
  int middle = bounds.size.h / 2;
  int spread = PBL_IF_ROUND_ELSE(bounds.size.h * 2 / 9, bounds.size.h / 4);
  switch (button) {
    case BUTTON_ID_UP: return GPoint(x, middle - spread);
    case BUTTON_ID_DOWN: return GPoint(x, middle + spread);
    default: return GPoint(bounds.size.w - PBL_IF_ROUND_ELSE(8, 6), middle);
  }
}
