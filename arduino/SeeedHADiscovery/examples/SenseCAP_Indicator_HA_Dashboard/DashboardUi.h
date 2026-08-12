#pragma once

#include <lvgl.h>

enum class DashboardMetric : uint8_t {
  Temperature,
  Humidity,
  CarbonDioxide,
  Tvoc,
};

void dashboardUiCreate();
void dashboardUiSetConnectionState(bool connected);
void dashboardUiSetRoomName(const char* roomName);
void dashboardUiSetMetric(DashboardMetric metric, const char* value,
                          const char* unit);
void dashboardUiSetTouchAvailable(bool available);
