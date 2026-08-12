#include "DashboardUi.h"

#include <Arduino.h>

namespace {

constexpr uint32_t kColorBackground = 0x0D151C;
constexpr uint32_t kColorSurface = 0x17232D;
constexpr uint32_t kColorSurfaceRaised = 0x1D2C37;
constexpr uint32_t kColorSurfacePressed = 0x263944;
constexpr uint32_t kColorTextPrimary = 0xF4F7F8;
constexpr uint32_t kColorTextSecondary = 0x92A5B1;
constexpr uint32_t kColorBorder = 0x2A3B46;
constexpr uint32_t kColorAccent = 0x53D8B0;
constexpr uint32_t kColorTemperature = 0xFF955F;
constexpr uint32_t kColorHumidity = 0x5CA8FF;
constexpr uint32_t kColorCo2 = 0xB17CFF;
constexpr uint32_t kColorEnergy = 0xFFB22E;
constexpr uint32_t kColorDanger = 0xD93434;
constexpr uint32_t kColorDangerPressed = 0xB52626;
constexpr uint32_t kColorOffline = 0x73838D;

constexpr uint8_t kPageCount = 3;
constexpr uint8_t kMetricCount = 6;
constexpr uint8_t kMetricViewCount = 2;
constexpr uint8_t kInteractiveCount = 16;
constexpr int16_t kControlIconX = 16;
constexpr int16_t kControlIconWidth = 38;
constexpr int16_t kControlTitleX = 68;

static_assert(kControlIconX + kControlIconWidth < kControlTitleX,
              "Control icon and title regions must not overlap");

enum class DashboardPage : uint8_t {
  Overview,
  Controls,
  Energy,
};

lv_obj_t* pages[kPageCount] = {nullptr, nullptr, nullptr};
lv_obj_t* navButtons[kPageCount] = {nullptr, nullptr, nullptr};
lv_obj_t* interactiveObjects[kInteractiveCount] = {};
uint8_t interactiveObjectCount = 0;

lv_obj_t* roomLabels[3] = {nullptr, nullptr, nullptr};
lv_obj_t* connectionDots[3] = {nullptr, nullptr, nullptr};
lv_obj_t* connectionLabels[3] = {nullptr, nullptr, nullptr};
lv_obj_t* occupancyLabel = nullptr;
lv_obj_t* batteryValueLabel = nullptr;
lv_obj_t* metricValues[kMetricCount][kMetricViewCount] = {};
lv_obj_t* metricUnits[kMetricCount][kMetricViewCount] = {};
uint8_t metricViewCounts[kMetricCount] = {};
lv_obj_t* windowStateLabels[2] = {nullptr, nullptr};
lv_obj_t* tvStateLabels[2] = {nullptr, nullptr};
lv_obj_t* confirmationOverlay = nullptr;
lv_obj_t* provisioningOverlay = nullptr;
lv_obj_t* noticePanel = nullptr;
lv_timer_t* noticeTimer = nullptr;
lv_obj_t* touchStatusLabel = nullptr;

DashboardPage activePage = DashboardPage::Overview;
DashboardActionCallback actionCallback = nullptr;
bool windowOpen = false;
bool tvPowerOn = false;
bool windowStateKnown = false;
bool tvPowerStateKnown = false;
bool touchAvailable = true;
bool controlsEnabled = false;

lv_color_t color(uint32_t hex) {
  return lv_color_hex(hex);
}

uint8_t pageIndex(DashboardPage page) {
  return static_cast<uint8_t>(page);
}

uint8_t metricIndex(DashboardMetric metric) {
  return static_cast<uint8_t>(metric);
}

// Registers one visual copy of a metric for synchronized updates.
// 注册同一指标的一处显示副本，后续更新时保持所有页面同步。
void registerMetricView(DashboardMetric metric, lv_obj_t* value,
                        lv_obj_t* unit) {
  const uint8_t index = metricIndex(metric);
  if (index >= kMetricCount || metricViewCounts[index] >= kMetricViewCount) {
    return;
  }
  const uint8_t view = metricViewCounts[index]++;
  metricValues[index][view] = value;
  metricUnits[index][view] = unit;
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

lv_obj_t* createPanel(lv_obj_t* parent, int16_t x, int16_t y,
                      int16_t width, int16_t height, uint8_t radius = 16) {
  lv_obj_t* panel = lv_obj_create(parent);
  lv_obj_set_pos(panel, x, y);
  lv_obj_set_size(panel, width, height);
  lv_obj_set_style_radius(panel, radius, 0);
  lv_obj_set_style_bg_color(panel, color(kColorSurface), 0);
  lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(panel, 1, 0);
  lv_obj_set_style_border_color(panel, color(kColorBorder), 0);
  lv_obj_set_style_pad_all(panel, 0, 0);
  lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
  return panel;
}

lv_obj_t* createWindowIcon(lv_obj_t* parent, int16_t y) {
  lv_obj_t* frame = lv_obj_create(parent);
  removeDefaultStyle(frame);
  lv_obj_set_pos(frame, kControlIconX, y);
  lv_obj_set_size(frame, kControlIconWidth, 42);
  lv_obj_set_style_radius(frame, 2, 0);
  lv_obj_set_style_border_width(frame, 3, 0);
  lv_obj_set_style_border_color(frame, color(kColorTextPrimary), 0);
  lv_obj_set_style_border_opa(frame, LV_OPA_COVER, 0);
  lv_obj_clear_flag(frame, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t* verticalDivider = lv_obj_create(frame);
  removeDefaultStyle(verticalDivider);
  lv_obj_set_pos(verticalDivider, 16, 0);
  lv_obj_set_size(verticalDivider, 3, 36);
  lv_obj_set_style_bg_color(verticalDivider, color(kColorTextPrimary), 0);
  lv_obj_set_style_bg_opa(verticalDivider, LV_OPA_COVER, 0);
  lv_obj_clear_flag(verticalDivider, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t* horizontalDivider = lv_obj_create(frame);
  removeDefaultStyle(horizontalDivider);
  lv_obj_set_pos(horizontalDivider, 0, 17);
  lv_obj_set_size(horizontalDivider, 32, 3);
  lv_obj_set_style_bg_color(horizontalDivider, color(kColorTextPrimary), 0);
  lv_obj_set_style_bg_opa(horizontalDivider, LV_OPA_COVER, 0);
  lv_obj_clear_flag(horizontalDivider, LV_OBJ_FLAG_CLICKABLE);
  return frame;
}

void registerInteractive(lv_obj_t* object) {
  if (interactiveObjectCount < kInteractiveCount) {
    interactiveObjects[interactiveObjectCount++] = object;
  }
}

void styleInteractive(lv_obj_t* object, bool danger = false) {
  lv_obj_add_flag(object, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_bg_color(
      object, color(danger ? kColorDangerPressed : kColorSurfacePressed),
      LV_STATE_PRESSED);
  lv_obj_set_style_border_color(
      object, color(danger ? 0xFF6767 : kColorAccent), LV_STATE_PRESSED);
  registerInteractive(object);
}

void createPageHeader(lv_obj_t* page, uint8_t index, const char* title) {
  roomLabels[index] = createLabel(page, title, &lv_font_montserrat_28,
                                  kColorTextPrimary);
  lv_obj_set_pos(roomLabels[index], 18, 13);

  connectionDots[index] = lv_obj_create(page);
  removeDefaultStyle(connectionDots[index]);
  lv_obj_set_pos(connectionDots[index], 348, 24);
  lv_obj_set_size(connectionDots[index], 8, 8);
  lv_obj_set_style_radius(connectionDots[index], LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(connectionDots[index], LV_OPA_COVER, 0);

  connectionLabels[index] = createLabel(page, "HA Offline",
                                         &lv_font_montserrat_14,
                                         kColorTextSecondary);
  lv_obj_set_pos(connectionLabels[index], 364, 19);
}

lv_obj_t* createMetricCard(lv_obj_t* parent, int16_t x, int16_t width,
                           const char* name, const char* value,
                           const char* unit, uint32_t accentColor,
                           DashboardMetric metric) {
  lv_obj_t* card = createPanel(parent, x, 84, width, 96);

  lv_obj_t* nameLabel = createLabel(card, name, &lv_font_montserrat_14,
                                    accentColor);
  lv_obj_set_pos(nameLabel, 14, 10);
  lv_obj_set_style_text_letter_space(nameLabel, 1, 0);

  lv_obj_t* valueLabel = createLabel(card, value, &lv_font_montserrat_28,
                                     accentColor);
  lv_obj_set_pos(valueLabel, 14, 42);

  lv_obj_t* unitLabel = createLabel(card, unit, &lv_font_montserrat_14,
                                    kColorTextSecondary);
  lv_obj_align_to(unitLabel, valueLabel,
                  LV_ALIGN_OUT_RIGHT_BOTTOM, 6, -3);
  registerMetricView(metric, valueLabel, unitLabel);
  return card;
}

void updateControlLabels() {
  const char* windowState = windowStateKnown
                                ? (windowOpen ? "Open" : "Closed")
                                : "Waiting for data";
  const char* tvState = tvPowerStateKnown
                            ? (tvPowerOn ? "On" : "Off")
                            : "Waiting for data";
  for (lv_obj_t* label : windowStateLabels) {
    if (label != nullptr) {
      lv_label_set_text(label, windowState);
      lv_obj_set_style_text_color(
          label,
          color(windowStateKnown && windowOpen
                    ? kColorAccent : kColorTextSecondary), 0);
    }
  }
  for (lv_obj_t* label : tvStateLabels) {
    if (label != nullptr) {
      lv_label_set_text(label, tvState);
      lv_obj_set_style_text_color(
          label,
          color(tvPowerStateKnown && tvPowerOn
                    ? kColorAccent : kColorTextSecondary), 0);
    }
  }
}

void invokeAction(DashboardAction action) {
  if (actionCallback != nullptr) {
    actionCallback(action);
  }
}

void closeNotice(lv_timer_t* timer) {
  if (noticePanel != nullptr) {
    lv_obj_delete(noticePanel);
    noticePanel = nullptr;
  }
  noticeTimer = nullptr;
  lv_timer_delete(timer);
}

// Shows short feedback without changing a device state locally.
// 显示短暂反馈，同时保持设备状态只由 Home Assistant 更新。
void showNotice(const char* message) {
  if (noticePanel != nullptr) {
    lv_obj_delete(noticePanel);
  }
  noticePanel = createPanel(lv_screen_active(), 110, 350, 260, 46, 14);
  lv_obj_set_style_bg_color(noticePanel, color(kColorSurfaceRaised), 0);
  lv_obj_move_foreground(noticePanel);
  lv_obj_t* label = createLabel(noticePanel, message,
                                &lv_font_montserrat_14, kColorTextPrimary);
  lv_obj_center(label);
  if (noticeTimer == nullptr) {
    noticeTimer = lv_timer_create(closeNotice, 1800, nullptr);
  } else {
    lv_timer_reset(noticeTimer);
  }
}

void handleWindowPressed(lv_event_t* event) {
  if (lv_event_get_code(event) != LV_EVENT_PRESSED || !touchAvailable) {
    return;
  }
  if (!controlsEnabled) {
    showNotice("HA unavailable");
    Serial.println("Dashboard window control unavailable");
    return;
  }
  showNotice("Sending to HA...");
  invokeAction(DashboardAction::WindowToggle);
  Serial.println("Dashboard window toggle requested");
}

void handleTvPressed(lv_event_t* event) {
  if (lv_event_get_code(event) != LV_EVENT_PRESSED || !touchAvailable) {
    return;
  }
  if (!controlsEnabled) {
    showNotice("HA unavailable");
    Serial.println("Dashboard TV control unavailable");
    return;
  }
  showNotice("Sending to HA...");
  invokeAction(DashboardAction::TvPowerToggle);
  Serial.println("Dashboard TV toggle requested");
}

void closeConfirmation() {
  if (confirmationOverlay != nullptr) {
    lv_obj_delete(confirmationOverlay);
    confirmationOverlay = nullptr;
  }
}

void handleConfirmationCancel(lv_event_t* event) {
  if (lv_event_get_code(event) == LV_EVENT_PRESSED) {
    closeConfirmation();
    Serial.println("Leave Room canceled");
  }
}

void handleConfirmationAccept(lv_event_t* event) {
  if (lv_event_get_code(event) != LV_EVENT_PRESSED) {
    return;
  }
  if (!controlsEnabled) {
    closeConfirmation();
    showNotice("HA unavailable");
    Serial.println("Leave Room control unavailable");
    return;
  }
  invokeAction(DashboardAction::LeaveRoom);
  closeConfirmation();
  showNotice("Sending to HA...");
  Serial.println("Leave Room turn-off requested");
}

void showLeaveConfirmation() {
  if (confirmationOverlay != nullptr) {
    return;
  }

  lv_obj_t* screen = lv_screen_active();
  confirmationOverlay = lv_obj_create(screen);
  lv_obj_set_pos(confirmationOverlay, 0, 0);
  lv_obj_set_size(confirmationOverlay, 480, 480);
  lv_obj_set_style_radius(confirmationOverlay, 0, 0);
  lv_obj_set_style_bg_color(confirmationOverlay, color(0x020609), 0);
  lv_obj_set_style_bg_opa(confirmationOverlay, LV_OPA_80, 0);
  lv_obj_set_style_border_width(confirmationOverlay, 0, 0);
  lv_obj_set_style_pad_all(confirmationOverlay, 0, 0);
  lv_obj_clear_flag(confirmationOverlay, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* dialog = createPanel(confirmationOverlay, 40, 126, 400, 228, 24);
  lv_obj_set_style_bg_color(dialog, color(kColorSurfaceRaised), 0);

  lv_obj_t* icon = createLabel(dialog, LV_SYMBOL_POWER,
                               &lv_font_montserrat_28, kColorDanger);
  lv_obj_set_pos(icon, 28, 26);
  lv_obj_t* title = createLabel(dialog, "Leave Room?",
                                &lv_font_montserrat_22, kColorTextPrimary);
  lv_obj_set_pos(title, 78, 27);
  lv_obj_t* detail = createLabel(
      dialog, "Turn off the configured\nroom devices.",
      &lv_font_montserrat_16, kColorTextSecondary);
  lv_obj_set_pos(detail, 28, 77);
  lv_obj_set_style_text_line_space(detail, 6, 0);

  lv_obj_t* cancelButton = createPanel(dialog, 28, 156, 158, 50, 16);
  styleInteractive(cancelButton);
  lv_obj_add_event_cb(cancelButton, handleConfirmationCancel,
                      LV_EVENT_PRESSED, nullptr);
  lv_obj_t* cancelLabel = createLabel(cancelButton, "Cancel",
                                      &lv_font_montserrat_16,
                                      kColorTextPrimary);
  lv_obj_center(cancelLabel);

  lv_obj_t* confirmButton = createPanel(dialog, 204, 156, 168, 50, 16);
  lv_obj_set_style_bg_color(confirmButton, color(kColorDanger), 0);
  styleInteractive(confirmButton, true);
  lv_obj_add_event_cb(confirmButton, handleConfirmationAccept,
                      LV_EVENT_PRESSED, nullptr);
  lv_obj_t* confirmLabel = createLabel(confirmButton, "Turn Off",
                                       &lv_font_montserrat_16,
                                       kColorTextPrimary);
  lv_obj_center(confirmLabel);
}

void handleLeavePressed(lv_event_t* event) {
  if (lv_event_get_code(event) == LV_EVENT_PRESSED && touchAvailable) {
    showLeaveConfirmation();
    Serial.println("Leave Room confirmation opened");
  }
}

void updateNavigation() {
  for (uint8_t i = 0; i < kPageCount; ++i) {
    const bool selected = i == pageIndex(activePage);
    if (selected) {
      lv_obj_clear_flag(pages[i], LV_OBJ_FLAG_HIDDEN);
    } else {
      lv_obj_add_flag(pages[i], LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_set_style_bg_opa(navButtons[i],
                            selected ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_color(navButtons[i], color(0x223347), 0);
    lv_obj_set_style_border_width(navButtons[i], 0, 0);
  }
}

void handleNavigation(lv_event_t* event) {
  if (lv_event_get_code(event) != LV_EVENT_PRESSED || !touchAvailable) {
    return;
  }
  lv_obj_t* target = static_cast<lv_obj_t*>(lv_event_get_target(event));
  for (uint8_t i = 0; i < kPageCount; ++i) {
    if (target == navButtons[i]) {
      activePage = static_cast<DashboardPage>(i);
      updateNavigation();
      Serial.printf("Dashboard page: %u\n", i);
      return;
    }
  }
}

lv_obj_t* createControlCard(lv_obj_t* parent, int16_t x, int16_t y,
                            int16_t width, int16_t height,
                            const char* iconText, bool useWindowIcon,
                            const char* title,
                            lv_obj_t** stateLabel,
                            lv_event_cb_t eventCallback) {
  lv_obj_t* card = createPanel(parent, x, y, width, height);
  styleInteractive(card);
  lv_obj_add_event_cb(card, eventCallback, LV_EVENT_PRESSED, nullptr);

  if (useWindowIcon) {
    createWindowIcon(card, (height - 42) / 2);
  } else {
    lv_obj_t* icon = createLabel(card, iconText, &lv_font_montserrat_22,
                                 kColorTextPrimary);
    lv_obj_set_pos(icon, kControlIconX, (height - 22) / 2);
  }
  lv_obj_t* titleLabel = createLabel(card, title, &lv_font_montserrat_18,
                                     kColorTextPrimary);
  lv_obj_set_pos(titleLabel, kControlTitleX, 12);
  *stateLabel = createLabel(card, "Off", &lv_font_montserrat_14,
                            kColorTextSecondary);
  lv_obj_set_pos(*stateLabel, kControlTitleX, 42);
  lv_obj_t* arrow = createLabel(card, LV_SYMBOL_RIGHT,
                                &lv_font_montserrat_18,
                                kColorTextSecondary);
  lv_obj_set_pos(arrow, width - 30, 27);
  return card;
}

void createOverviewPage(lv_obj_t* screen) {
  lv_obj_t* page = lv_obj_create(screen);
  removeDefaultStyle(page);
  lv_obj_set_pos(page, 0, 0);
  lv_obj_set_size(page, 480, 412);
  lv_obj_clear_flag(page, LV_OBJ_FLAG_SCROLLABLE);
  pages[pageIndex(DashboardPage::Overview)] = page;
  createPageHeader(page, 0, "ROOM 106");

  occupancyLabel = createLabel(page, "Waiting for data",
                                &lv_font_montserrat_18,
                                kColorTextSecondary);
  lv_obj_set_pos(occupancyLabel, 19, 50);
  lv_obj_t* batteryIcon = createLabel(page, LV_SYMBOL_BATTERY_FULL,
                                      &lv_font_montserrat_22, kColorAccent);
  lv_obj_set_pos(batteryIcon, 336, 49);
  lv_obj_t* batteryName = createLabel(page, "Motion", &lv_font_montserrat_14,
                                      kColorTextSecondary);
  lv_obj_set_pos(batteryName, 368, 46);
  batteryValueLabel = createLabel(page, "--%", &lv_font_montserrat_18,
                                  kColorTextPrimary);
  lv_obj_set_pos(batteryValueLabel, 368, 62);

  createMetricCard(page, 16, 140, "CO2", "--", "ppm",
                   kColorCo2, DashboardMetric::CarbonDioxide);
  createMetricCard(page, 164, 146, "TEMPERATURE", "--", "C",
                   kColorTemperature, DashboardMetric::Temperature);
  createMetricCard(page, 318, 146, "HUMIDITY", "--", "%",
                   kColorHumidity, DashboardMetric::Humidity);

  createControlCard(page, 16, 190, 220, 78, nullptr, true,
                    "Window", &windowStateLabels[0], handleWindowPressed);
  createControlCard(page, 244, 190, 220, 78, LV_SYMBOL_POWER, false,
                    "TV Power", &tvStateLabels[0], handleTvPressed);

  lv_obj_t* energyCard = createPanel(page, 16, 278, 448, 50);
  lv_obj_t* energyIcon = createLabel(energyCard, LV_SYMBOL_CHARGE,
                                     &lv_font_montserrat_18, kColorEnergy);
  lv_obj_set_pos(energyIcon, 17, 15);
  lv_obj_t* energyName = createLabel(energyCard, "Month",
                                     &lv_font_montserrat_16,
                                     kColorTextSecondary);
  lv_obj_set_pos(energyName, 51, 15);
  lv_obj_t* monthValue = createLabel(energyCard, "--",
                                     &lv_font_montserrat_22, kColorEnergy);
  lv_obj_set_pos(monthValue, 330, 11);
  lv_obj_t* monthUnit = createLabel(energyCard, "kWh",
                                    &lv_font_montserrat_14, kColorEnergy);
  lv_obj_align_to(monthUnit, monthValue, LV_ALIGN_OUT_RIGHT_BOTTOM, 6, -3);
  registerMetricView(DashboardMetric::MonthlyEnergy, monthValue, monthUnit);

  lv_obj_t* leaveButton = createPanel(page, 16, 338, 448, 58, 18);
  lv_obj_set_style_bg_color(leaveButton, color(kColorDanger), 0);
  styleInteractive(leaveButton, true);
  lv_obj_add_event_cb(leaveButton, handleLeavePressed,
                      LV_EVENT_PRESSED, nullptr);
  lv_obj_t* leaveLabel = createLabel(leaveButton,
                                     LV_SYMBOL_POWER "  Leave Room",
                                     &lv_font_montserrat_18,
                                     kColorTextPrimary);
  lv_obj_center(leaveLabel);
}

void createControlsPage(lv_obj_t* screen) {
  lv_obj_t* page = lv_obj_create(screen);
  removeDefaultStyle(page);
  lv_obj_set_pos(page, 0, 0);
  lv_obj_set_size(page, 480, 412);
  lv_obj_clear_flag(page, LV_OBJ_FLAG_SCROLLABLE);
  pages[pageIndex(DashboardPage::Controls)] = page;
  createPageHeader(page, 1, "CONTROLS");

  lv_obj_t* subtitle = createLabel(page, "ROOM 106 DEVICES",
                                   &lv_font_montserrat_14,
                                   kColorTextSecondary);
  lv_obj_set_pos(subtitle, 18, 58);

  createControlCard(page, 16, 92, 448, 88, nullptr, true,
                    "Electric Window", &windowStateLabels[1],
                    handleWindowPressed);
  createControlCard(page, 16, 190, 448, 88, LV_SYMBOL_POWER, false,
                    "Television Power", &tvStateLabels[1], handleTvPressed);

  lv_obj_t* note = createPanel(page, 16, 288, 448, 44, 14);
  lv_obj_t* noteLabel = createLabel(note, "Controls will sync with HA",
                                    &lv_font_montserrat_14,
                                    kColorTextSecondary);
  lv_obj_center(noteLabel);

  lv_obj_t* leaveButton = createPanel(page, 16, 342, 448, 54, 18);
  lv_obj_set_style_bg_color(leaveButton, color(kColorDanger), 0);
  styleInteractive(leaveButton, true);
  lv_obj_add_event_cb(leaveButton, handleLeavePressed,
                      LV_EVENT_PRESSED, nullptr);
  lv_obj_t* leaveLabel = createLabel(leaveButton,
                                     LV_SYMBOL_POWER "  Leave Room",
                                     &lv_font_montserrat_18,
                                     kColorTextPrimary);
  lv_obj_center(leaveLabel);
}

void createEnergyPage(lv_obj_t* screen) {
  lv_obj_t* page = lv_obj_create(screen);
  removeDefaultStyle(page);
  lv_obj_set_pos(page, 0, 0);
  lv_obj_set_size(page, 480, 412);
  lv_obj_clear_flag(page, LV_OBJ_FLAG_SCROLLABLE);
  pages[pageIndex(DashboardPage::Energy)] = page;
  createPageHeader(page, 2, "ENERGY");

  lv_obj_t* hero = createPanel(page, 16, 84, 448, 132, 22);
  lv_obj_t* heroLabel = createLabel(hero, "THIS MONTH",
                                    &lv_font_montserrat_14,
                                    kColorTextSecondary);
  lv_obj_set_pos(heroLabel, 22, 20);
  lv_obj_t* heroValue = createLabel(hero, "--",
                                    &lv_font_montserrat_36, kColorEnergy);
  lv_obj_set_pos(heroValue, 22, 55);
  lv_obj_t* heroUnit = createLabel(hero, "kWh", &lv_font_montserrat_18,
                                   kColorEnergy);
  lv_obj_align_to(heroUnit, heroValue, LV_ALIGN_OUT_RIGHT_BOTTOM, 8, -5);
  registerMetricView(DashboardMetric::MonthlyEnergy, heroValue, heroUnit);

  lv_obj_t* today = createPanel(page, 16, 228, 216, 96);
  lv_obj_t* todayName = createLabel(today, "TODAY",
                                    &lv_font_montserrat_14,
                                    kColorTextSecondary);
  lv_obj_set_pos(todayName, 18, 16);
  lv_obj_t* todayValue = createLabel(today, "--",
                                     &lv_font_montserrat_22,
                                     kColorTextPrimary);
  lv_obj_set_pos(todayValue, 18, 51);
  lv_obj_t* todayUnit = createLabel(today, "kWh", &lv_font_montserrat_14,
                                    kColorTextSecondary);
  lv_obj_align_to(todayUnit, todayValue, LV_ALIGN_OUT_RIGHT_BOTTOM, 6, -3);
  registerMetricView(DashboardMetric::TodayEnergy, todayValue, todayUnit);

  lv_obj_t* power = createPanel(page, 244, 228, 220, 96);
  lv_obj_t* powerName = createLabel(power, "TV POWER",
                                    &lv_font_montserrat_14,
                                    kColorTextSecondary);
  lv_obj_set_pos(powerName, 18, 16);
  lv_obj_t* powerValue = createLabel(power, "--",
                                     &lv_font_montserrat_22,
                                     kColorTextPrimary);
  lv_obj_set_pos(powerValue, 18, 51);
  lv_obj_t* powerUnit = createLabel(power, "W", &lv_font_montserrat_14,
                                    kColorTextSecondary);
  lv_obj_align_to(powerUnit, powerValue, LV_ALIGN_OUT_RIGHT_BOTTOM, 6, -3);
  registerMetricView(DashboardMetric::CurrentPower, powerValue, powerUnit);

  lv_obj_t* note = createLabel(page, "Live values will arrive from HA",
                               &lv_font_montserrat_14,
                               kColorTextSecondary);
  lv_obj_set_pos(note, 18, 350);
}

void createNavigation(lv_obj_t* screen) {
  lv_obj_t* bar = lv_obj_create(screen);
  lv_obj_set_pos(bar, 0, 412);
  lv_obj_set_size(bar, 480, 68);
  lv_obj_set_style_radius(bar, 0, 0);
  lv_obj_set_style_bg_color(bar, color(0x111D27), 0);
  lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(bar, 1, 0);
  lv_obj_set_style_border_side(bar, LV_BORDER_SIDE_TOP, 0);
  lv_obj_set_style_border_color(bar, color(kColorBorder), 0);
  lv_obj_set_style_pad_all(bar, 0, 0);
  lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

  constexpr const char* labels[kPageCount] = {
      LV_SYMBOL_HOME "  Overview",
      LV_SYMBOL_SETTINGS "  Controls",
      LV_SYMBOL_CHARGE "  Energy",
  };

  for (uint8_t i = 0; i < kPageCount; ++i) {
    navButtons[i] = lv_obj_create(bar);
    lv_obj_set_pos(navButtons[i], 6 + i * 158, 7);
    lv_obj_set_size(navButtons[i], 152, 54);
    lv_obj_set_style_radius(navButtons[i], 18, 0);
    lv_obj_set_style_pad_all(navButtons[i], 0, 0);
    lv_obj_clear_flag(navButtons[i], LV_OBJ_FLAG_SCROLLABLE);
    styleInteractive(navButtons[i]);
    lv_obj_add_event_cb(navButtons[i], handleNavigation,
                        LV_EVENT_PRESSED, nullptr);
    lv_obj_t* label = createLabel(navButtons[i], labels[i],
                                  &lv_font_montserrat_14,
                                  kColorTextPrimary);
    lv_obj_center(label);
  }
}

}  // namespace

void dashboardUiCreate() {
  lv_obj_t* screen = lv_screen_active();
  lv_obj_set_style_bg_color(screen, color(kColorBackground), 0);
  lv_obj_set_style_bg_grad_color(screen, color(0x111E27), 0);
  lv_obj_set_style_bg_grad_dir(screen, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
  lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

  createOverviewPage(screen);
  createControlsPage(screen);
  createEnergyPage(screen);
  createNavigation(screen);
  updateControlLabels();
  updateNavigation();
  dashboardUiSetConnectionState(DashboardConnectionState::Offline);
}

void dashboardUiSetConnectionState(DashboardConnectionState state) {
  const char* text = "HA Offline";
  uint32_t statusColor = kColorOffline;
  if (state == DashboardConnectionState::Provisioning) {
    text = "WiFi Setup";
    statusColor = kColorEnergy;
  } else if (state == DashboardConnectionState::WaitingForHA) {
    text = "HA Waiting";
    statusColor = kColorHumidity;
  } else if (state == DashboardConnectionState::Online) {
    text = "HA Online";
    statusColor = kColorAccent;
  }

  for (uint8_t i = 0; i < kPageCount; ++i) {
    if (connectionDots[i] == nullptr || connectionLabels[i] == nullptr) {
      continue;
    }
    lv_obj_set_style_bg_color(connectionDots[i], color(statusColor), 0);
    lv_label_set_text(connectionLabels[i], text);
    lv_obj_set_style_text_color(connectionLabels[i], color(statusColor), 0);
  }
}

void dashboardUiSetProvisioningState(bool active, const char* accessPoint,
                                     const char* address) {
  if (!active) {
    if (provisioningOverlay != nullptr) {
      lv_obj_delete(provisioningOverlay);
      provisioningOverlay = nullptr;
    }
    return;
  }
  if (provisioningOverlay != nullptr) {
    return;
  }

  provisioningOverlay = lv_obj_create(lv_screen_active());
  lv_obj_set_pos(provisioningOverlay, 0, 0);
  lv_obj_set_size(provisioningOverlay, 480, 480);
  lv_obj_set_style_radius(provisioningOverlay, 0, 0);
  lv_obj_set_style_bg_color(provisioningOverlay, color(kColorBackground), 0);
  lv_obj_set_style_bg_opa(provisioningOverlay, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(provisioningOverlay, 0, 0);
  lv_obj_set_style_pad_all(provisioningOverlay, 0, 0);
  lv_obj_clear_flag(provisioningOverlay, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* card = createPanel(provisioningOverlay, 40, 76, 400, 328, 24);
  lv_obj_set_style_bg_color(card, color(kColorSurfaceRaised), 0);
  lv_obj_t* icon = createLabel(card, LV_SYMBOL_WIFI,
                               &lv_font_montserrat_36, kColorEnergy);
  lv_obj_set_pos(icon, 174, 30);
  lv_obj_t* title = createLabel(card, "WiFi Setup",
                                &lv_font_montserrat_28, kColorTextPrimary);
  lv_obj_set_pos(title, 118, 86);
  lv_obj_t* detail = createLabel(
      card, "1. Connect to this hotspot\n2. Open the setup address",
      &lv_font_montserrat_16, kColorTextSecondary);
  lv_obj_set_pos(detail, 44, 137);
  lv_obj_set_style_text_line_space(detail, 10, 0);

  lv_obj_t* apLabel = createLabel(card,
                                  accessPoint != nullptr ? accessPoint : "--",
                                  &lv_font_montserrat_18, kColorAccent);
  lv_obj_set_pos(apLabel, 44, 218);
  lv_obj_t* addressLabel = createLabel(
      card, address != nullptr ? address : "192.168.4.1",
      &lv_font_montserrat_16, kColorHumidity);
  lv_obj_set_pos(addressLabel, 44, 258);
}

void dashboardUiSetRoomName(const char* roomName) {
  if (roomName == nullptr) {
    return;
  }
  lv_label_set_text(roomLabels[0], roomName);
}

void dashboardUiSetOccupancyState(const char* state, bool occupied) {
  if (occupancyLabel == nullptr || state == nullptr) {
    return;
  }
  lv_label_set_text(occupancyLabel, state);
  lv_obj_set_style_text_color(
      occupancyLabel, color(occupied ? kColorAccent : kColorTextSecondary), 0);
}

void dashboardUiSetMotionBattery(const char* value) {
  if (batteryValueLabel != nullptr && value != nullptr) {
    lv_label_set_text(batteryValueLabel, value);
  }
}

void dashboardUiSetMetric(DashboardMetric metric, const char* value,
                          const char* unit) {
  const uint8_t index = metricIndex(metric);
  if (index >= kMetricCount || value == nullptr) {
    return;
  }
  for (uint8_t view = 0; view < metricViewCounts[index]; ++view) {
    if (metricValues[index][view] == nullptr) {
      continue;
    }
    lv_label_set_text(metricValues[index][view], value);
    if (unit != nullptr && metricUnits[index][view] != nullptr) {
      lv_label_set_text(metricUnits[index][view], unit);
      lv_obj_align_to(metricUnits[index][view], metricValues[index][view],
                      LV_ALIGN_OUT_RIGHT_BOTTOM,
                      metric == DashboardMetric::MonthlyEnergy && view == 1
                          ? 8 : 6,
                      metric == DashboardMetric::MonthlyEnergy && view == 1
                          ? -5 : -3);
    }
  }
}

void dashboardUiSetWindowState(bool open) {
  windowOpen = open;
  windowStateKnown = true;
  updateControlLabels();
}

void dashboardUiSetTvPowerState(bool on) {
  tvPowerOn = on;
  tvPowerStateKnown = true;
  updateControlLabels();
}

void dashboardUiSetTouchAvailable(bool available) {
  touchAvailable = available;
  for (uint8_t i = 0; i < interactiveObjectCount; ++i) {
    if (available) {
      lv_obj_add_flag(interactiveObjects[i], LV_OBJ_FLAG_CLICKABLE);
    } else {
      lv_obj_clear_flag(interactiveObjects[i], LV_OBJ_FLAG_CLICKABLE);
    }
  }

  if (!available && touchStatusLabel == nullptr) {
    touchStatusLabel = createLabel(lv_screen_active(), "Touch unavailable",
                                   &lv_font_montserrat_14, kColorDanger);
    lv_obj_set_pos(touchStatusLabel, 170, 392);
  } else if (available && touchStatusLabel != nullptr) {
    lv_obj_delete(touchStatusLabel);
    touchStatusLabel = nullptr;
  }
}

void dashboardUiSetControlsEnabled(bool enabled) {
  controlsEnabled = enabled;
}

void dashboardUiShowNotice(const char* message) {
  if (message != nullptr && message[0] != '\0') {
    showNotice(message);
  }
}

void dashboardUiOnAction(DashboardActionCallback callback) {
  actionCallback = callback;
}
