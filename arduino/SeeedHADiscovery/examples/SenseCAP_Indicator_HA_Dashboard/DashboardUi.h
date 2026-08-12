#pragma once

#include <lvgl.h>

enum class DashboardMetric : uint8_t {
  Temperature,
  Humidity,
  CarbonDioxide,
  MonthlyEnergy,
};

enum class DashboardAction : uint8_t {
  WindowToggle,
  TvPowerToggle,
  LeaveRoom,
};

using DashboardActionCallback = void (*)(DashboardAction action);

void dashboardUiCreate();
void dashboardUiSetConnectionState(bool connected);
void dashboardUiSetRoomName(const char* roomName);
void dashboardUiSetOccupancyState(const char* state, bool occupied);
void dashboardUiSetMotionBattery(const char* value);
void dashboardUiSetMetric(DashboardMetric metric, const char* value,
                          const char* unit);
void dashboardUiSetWindowState(bool open);
void dashboardUiSetTvPowerState(bool on);
void dashboardUiSetTouchAvailable(bool available);
void dashboardUiOnAction(DashboardActionCallback callback);
