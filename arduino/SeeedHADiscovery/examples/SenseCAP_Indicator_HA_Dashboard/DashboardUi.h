#pragma once

#include <lvgl.h>

enum class DashboardMetric : uint8_t {
  Temperature,
  Humidity,
  CarbonDioxide,
  MonthlyEnergy,
  TodayEnergy,
  CurrentPower,
};

enum class DashboardConnectionState : uint8_t {
  Offline,
  Provisioning,
  WaitingForHA,
  Online,
};

enum class DashboardAction : uint8_t {
  WindowToggle,
  TvPowerToggle,
  LeftSwitchToggle,
  RightSwitchToggle,
  LeaveRoom,
};

using DashboardActionCallback = void (*)(DashboardAction action);

void dashboardUiCreate();
void dashboardUiSetConnectionState(DashboardConnectionState state);
void dashboardUiSetProvisioningState(bool active, const char* accessPoint,
                                     const char* address);
void dashboardUiSetRoomName(const char* roomName);
void dashboardUiSetOccupancyState(const char* state, bool occupied);
void dashboardUiSetMotionBattery(const char* value);
void dashboardUiSetMetric(DashboardMetric metric, const char* value,
                          const char* unit);
void dashboardUiSetWindowState(bool open);
void dashboardUiSetTvPowerState(bool on);
void dashboardUiSetLeftSwitchState(bool on);
void dashboardUiSetRightSwitchState(bool on);
void dashboardUiSetTouchAvailable(bool available);
void dashboardUiSetControlsEnabled(bool enabled);
void dashboardUiShowNotice(const char* message);
void dashboardUiOnAction(DashboardActionCallback callback);
