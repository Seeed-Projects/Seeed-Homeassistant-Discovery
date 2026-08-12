#include "DashboardUi.h"

#include <Arduino.h>

namespace {

constexpr uint32_t kColorBackground = 0x0D151C;
constexpr uint32_t kColorSurface = 0x17232D;
constexpr uint32_t kColorSurfaceRaised = 0x1D2C37;
constexpr uint32_t kColorTextPrimary = 0xF4F7F8;
constexpr uint32_t kColorTextSecondary = 0x92A5B1;
constexpr uint32_t kColorBorder = 0x2A3B46;
constexpr uint32_t kColorAccent = 0x53D8B0;
constexpr uint32_t kColorTemperature = 0xFF9B73;
constexpr uint32_t kColorHumidity = 0x6CB8FF;
constexpr uint32_t kColorCo2 = 0xA998FF;
constexpr uint32_t kColorTvoc = 0xF4C95D;
constexpr uint32_t kColorOffline = 0x73838D;

lv_obj_t* connectionPill = nullptr;
lv_obj_t* connectionDot = nullptr;
lv_obj_t* connectionLabel = nullptr;
lv_obj_t* roomLabel = nullptr;
lv_obj_t* metricValues[4] = {nullptr, nullptr, nullptr, nullptr};
lv_obj_t* metricUnits[4] = {nullptr, nullptr, nullptr, nullptr};
lv_obj_t* touchCard = nullptr;
lv_obj_t* touchTitle = nullptr;
lv_obj_t* touchDetail = nullptr;
lv_obj_t* touchCountLabel = nullptr;
uint32_t touchCount = 0;

lv_color_t color(uint32_t hex) {
  return lv_color_hex(hex);
}

void removeDefaultStyle(lv_obj_t* object) {
  lv_obj_remove_style_all(object);
  lv_obj_set_style_bg_opa(object, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(object, 0, 0);
  lv_obj_set_style_pad_all(object, 0, 0);
}

lv_obj_t* createLabel(lv_obj_t* parent, const char* text,
                      const lv_font_t* font, uint32_t textColor) {
  lv_obj_t* label = lv_label_create(parent);
  removeDefaultStyle(label);
  lv_label_set_text(label, text);
  lv_obj_set_style_text_font(label, font, 0);
  lv_obj_set_style_text_color(label, color(textColor), 0);
  return label;
}

lv_obj_t* createMetricCard(lv_obj_t* parent, int16_t x, int16_t y,
                           const char* name, const char* unit,
                           uint32_t accentColor, uint8_t index) {
  lv_obj_t* card = lv_obj_create(parent);
  lv_obj_set_pos(card, x, y);
  lv_obj_set_size(card, 210, 112);
  lv_obj_set_style_radius(card, 22, 0);
  lv_obj_set_style_bg_color(card, color(kColorSurface), 0);
  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(card, 1, 0);
  lv_obj_set_style_border_color(card, color(kColorBorder), 0);
  lv_obj_set_style_pad_all(card, 18, 0);
  lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* accent = lv_obj_create(card);
  removeDefaultStyle(accent);
  lv_obj_set_size(accent, 8, 8);
  lv_obj_set_pos(accent, 0, 2);
  lv_obj_set_style_radius(accent, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(accent, color(accentColor), 0);
  lv_obj_set_style_bg_opa(accent, LV_OPA_COVER, 0);

  lv_obj_t* nameLabel = createLabel(card, name, &lv_font_montserrat_14,
                                    kColorTextSecondary);
  lv_obj_set_pos(nameLabel, 18, -2);
  lv_obj_set_style_text_letter_space(nameLabel, 1, 0);

  metricValues[index] = createLabel(card, "--", &lv_font_montserrat_36,
                                    kColorTextPrimary);
  lv_obj_set_pos(metricValues[index], 0, 30);

  metricUnits[index] = createLabel(card, unit, &lv_font_montserrat_16,
                                   kColorTextSecondary);
  lv_obj_align_to(metricUnits[index], metricValues[index],
                  LV_ALIGN_OUT_RIGHT_BOTTOM, 8, -5);
  return card;
}

void handleTouchCard(lv_event_t* event) {
  const lv_event_code_t eventCode = lv_event_get_code(event);
  if (eventCode == LV_EVENT_PRESSED) {
    Serial.println("LVGL card pressed");
    ++touchCount;
    lv_label_set_text(touchTitle, "Touch confirmed");
    lv_label_set_text(touchDetail, "Input is aligned and ready");
    lv_label_set_text_fmt(touchCountLabel, "%lu",
                          static_cast<unsigned long>(touchCount));
    Serial.printf("Touch test count: %lu\n",
                  static_cast<unsigned long>(touchCount));
    return;
  }
  if (eventCode == LV_EVENT_RELEASED) {
    Serial.println("LVGL card released");
    return;
  }
  if (eventCode != LV_EVENT_CLICKED) {
    return;
  }

  Serial.println("LVGL card clicked");
}

uint8_t metricIndex(DashboardMetric metric) {
  return static_cast<uint8_t>(metric);
}

}  // namespace

void dashboardUiCreate() {
  lv_obj_t* screen = lv_screen_active();
  lv_obj_set_style_bg_color(screen, color(kColorBackground), 0);
  lv_obj_set_style_bg_grad_color(screen, color(0x101F27), 0);
  lv_obj_set_style_bg_grad_dir(screen, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
  lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* eyebrow = createLabel(screen, "MEETING ROOM",
                                  &lv_font_montserrat_14,
                                  kColorTextSecondary);
  lv_obj_set_pos(eyebrow, 26, 22);

  roomLabel = createLabel(screen, "Atlas / 4F", &lv_font_montserrat_28,
                          kColorTextPrimary);
  lv_obj_set_pos(roomLabel, 24, 44);

  connectionPill = lv_obj_create(screen);
  lv_obj_set_pos(connectionPill, 348, 28);
  lv_obj_set_size(connectionPill, 106, 38);
  lv_obj_set_style_radius(connectionPill, 19, 0);
  lv_obj_set_style_bg_color(connectionPill, color(kColorSurfaceRaised), 0);
  lv_obj_set_style_bg_opa(connectionPill, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(connectionPill, 1, 0);
  lv_obj_set_style_border_color(connectionPill, color(kColorBorder), 0);
  lv_obj_set_style_pad_all(connectionPill, 0, 0);
  lv_obj_clear_flag(connectionPill, LV_OBJ_FLAG_SCROLLABLE);

  connectionDot = lv_obj_create(connectionPill);
  removeDefaultStyle(connectionDot);
  lv_obj_set_size(connectionDot, 8, 8);
  lv_obj_set_pos(connectionDot, 16, 14);
  lv_obj_set_style_radius(connectionDot, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(connectionDot, LV_OPA_COVER, 0);

  connectionLabel = createLabel(connectionPill, "Offline",
                                &lv_font_montserrat_14,
                                kColorTextSecondary);
  lv_obj_set_pos(connectionLabel, 34, 10);
  dashboardUiSetConnectionState(false);

  lv_obj_t* divider = lv_obj_create(screen);
  removeDefaultStyle(divider);
  lv_obj_set_pos(divider, 24, 91);
  lv_obj_set_size(divider, 432, 1);
  lv_obj_set_style_bg_color(divider, color(kColorBorder), 0);
  lv_obj_set_style_bg_opa(divider, LV_OPA_COVER, 0);

  createMetricCard(screen, 24, 112, "TEMPERATURE", "C",
                   kColorTemperature, 0);
  createMetricCard(screen, 246, 112, "HUMIDITY", "%",
                   kColorHumidity, 1);
  createMetricCard(screen, 24, 234, "CO2", "ppm",
                   kColorCo2, 2);
  createMetricCard(screen, 246, 234, "TVOC", "ppb",
                   kColorTvoc, 3);

  touchCard = lv_obj_create(screen);
  lv_obj_set_pos(touchCard, 24, 370);
  lv_obj_set_size(touchCard, 432, 86);
  lv_obj_set_style_radius(touchCard, 22, 0);
  lv_obj_set_style_bg_color(touchCard, color(kColorSurfaceRaised), 0);
  lv_obj_set_style_bg_color(touchCard, color(0x243D3B), LV_STATE_PRESSED);
  lv_obj_set_style_bg_opa(touchCard, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(touchCard, 1, 0);
  lv_obj_set_style_border_color(touchCard, color(kColorBorder), 0);
  lv_obj_set_style_border_color(touchCard, color(kColorAccent),
                                LV_STATE_PRESSED);
  lv_obj_set_style_pad_all(touchCard, 0, 0);
  lv_obj_add_flag(touchCard, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(touchCard, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(touchCard, handleTouchCard, LV_EVENT_ALL, nullptr);

  lv_obj_t* touchIcon = lv_obj_create(touchCard);
  removeDefaultStyle(touchIcon);
  lv_obj_set_pos(touchIcon, 16, 17);
  lv_obj_set_size(touchIcon, 52, 52);
  lv_obj_set_style_radius(touchIcon, 16, 0);
  lv_obj_set_style_bg_color(touchIcon, color(0x244139), 0);
  lv_obj_set_style_bg_opa(touchIcon, LV_OPA_COVER, 0);
  lv_obj_clear_flag(touchIcon, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t* touchMark = createLabel(touchIcon, "+", &lv_font_montserrat_28,
                                    kColorAccent);
  lv_obj_center(touchMark);

  touchTitle = createLabel(touchCard, "Touch screen",
                           &lv_font_montserrat_18, kColorTextPrimary);
  lv_obj_set_pos(touchTitle, 84, 18);
  touchDetail = createLabel(touchCard, "Tap anywhere on this card",
                            &lv_font_montserrat_14, kColorTextSecondary);
  lv_obj_set_pos(touchDetail, 84, 47);

  lv_obj_t* countPill = lv_obj_create(touchCard);
  lv_obj_set_pos(countPill, 364, 24);
  lv_obj_set_size(countPill, 48, 38);
  lv_obj_set_style_radius(countPill, 19, 0);
  lv_obj_set_style_bg_color(countPill, color(0x244139), 0);
  lv_obj_set_style_bg_opa(countPill, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(countPill, 0, 0);
  lv_obj_set_style_pad_all(countPill, 0, 0);
  lv_obj_clear_flag(countPill, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_clear_flag(countPill, LV_OBJ_FLAG_SCROLLABLE);

  touchCountLabel = createLabel(countPill, "0", &lv_font_montserrat_16,
                                kColorAccent);
  lv_obj_center(touchCountLabel);
}

void dashboardUiSetConnectionState(bool connected) {
  if (connectionDot == nullptr || connectionLabel == nullptr) {
    return;
  }
  lv_obj_set_style_bg_color(connectionDot,
                            color(connected ? kColorAccent : kColorOffline), 0);
  lv_label_set_text(connectionLabel, connected ? "Online" : "Offline");
  lv_obj_set_style_text_color(
      connectionLabel,
      color(connected ? kColorAccent : kColorTextSecondary), 0);
}

void dashboardUiSetRoomName(const char* roomName) {
  if (roomLabel != nullptr && roomName != nullptr) {
    lv_label_set_text(roomLabel, roomName);
  }
}

void dashboardUiSetMetric(DashboardMetric metric, const char* value,
                          const char* unit) {
  const uint8_t index = metricIndex(metric);
  if (index >= 4 || value == nullptr || metricValues[index] == nullptr) {
    return;
  }
  lv_label_set_text(metricValues[index], value);
  if (unit != nullptr && metricUnits[index] != nullptr) {
    lv_label_set_text(metricUnits[index], unit);
  }
  lv_obj_align_to(metricUnits[index], metricValues[index],
                  LV_ALIGN_OUT_RIGHT_BOTTOM, 8, -5);
}

void dashboardUiSetTouchAvailable(bool available) {
  if (touchCard == nullptr) {
    return;
  }
  if (available) {
    lv_label_set_text(touchTitle, "Touch screen");
    lv_label_set_text(touchDetail, "Tap anywhere on this card");
    lv_obj_add_flag(touchCard, LV_OBJ_FLAG_CLICKABLE);
  } else {
    lv_label_set_text(touchTitle, "Touch unavailable");
    lv_label_set_text(touchDetail, "Check the touch controller");
    lv_obj_clear_flag(touchCard, LV_OBJ_FLAG_CLICKABLE);
  }
}
