#include "RoomDashboardState.h"

#include <Arduino.h>

#include "DashboardUi.h"
#include "RoomDashboardConfig.h"

namespace {

constexpr size_t kStateLength = 24;

enum StateChange : uint16_t {
  kOccupancyChanged = 1 << 0,
  kBatteryChanged = 1 << 1,
  kCarbonDioxideChanged = 1 << 2,
  kTemperatureChanged = 1 << 3,
  kHumidityChanged = 1 << 4,
  kWindowChanged = 1 << 5,
  kTvPowerChanged = 1 << 6,
  kCurrentPowerChanged = 1 << 7,
  kTodayEnergyChanged = 1 << 8,
  kMonthlyEnergyChanged = 1 << 9,
};

struct RoomStateCache {
  char occupancy[kStateLength] = {};
  char battery[kStateLength] = {};
  char carbonDioxide[kStateLength] = {};
  char temperature[kStateLength] = {};
  char humidity[kStateLength] = {};
  char currentPower[kStateLength] = {};
  char todayEnergy[kStateLength] = {};
  char monthlyEnergy[kStateLength] = {};
  bool occupied = false;
  bool windowOpen = false;
  bool tvPowerOn = false;
  uint16_t changes = 0;
};

RoomStateCache cache;

bool matchesEntity(const char* actual, const char* configured) {
  return actual != nullptr && configured != nullptr &&
         strcmp(actual, configured) == 0;
}

void copyState(char* destination, const char* source) {
  snprintf(destination, kStateLength, "%s",
           source != nullptr && source[0] != '\0' ? source : "--");
}

bool equalsIgnoreCase(const char* left, const char* right) {
  return left != nullptr && right != nullptr && strcasecmp(left, right) == 0;
}

bool stateIsUnavailable(const char* state) {
  return state == nullptr || state[0] == '\0' ||
         equalsIgnoreCase(state, "unknown") ||
         equalsIgnoreCase(state, "unavailable") ||
         equalsIgnoreCase(state, "none");
}

bool stateIsOn(const char* state) {
  return equalsIgnoreCase(state, "on") ||
         equalsIgnoreCase(state, "open") ||
         equalsIgnoreCase(state, "opening") ||
         equalsIgnoreCase(state, "occupied") ||
         equalsIgnoreCase(state, "detected") ||
         equalsIgnoreCase(state, "home") ||
         equalsIgnoreCase(state, "true") ||
         equalsIgnoreCase(state, "1");
}

void updateOccupancy(const char* state) {
  if (stateIsUnavailable(state)) {
    copyState(cache.occupancy, "Unknown");
    cache.occupied = false;
  } else {
    cache.occupied = stateIsOn(state);
    copyState(cache.occupancy, cache.occupied ? "Occupied" : "Vacant");
  }
  cache.changes |= kOccupancyChanged;
}

void updateBattery(const char* state) {
  if (stateIsUnavailable(state)) {
    copyState(cache.battery, "--%");
  } else if (strchr(state, '%') != nullptr) {
    copyState(cache.battery, state);
  } else {
    snprintf(cache.battery, kStateLength, "%s%%", state);
  }
  cache.changes |= kBatteryChanged;
}

void updateMetric(char* destination, const char* state,
                  StateChange change) {
  copyState(destination, stateIsUnavailable(state) ? "--" : state);
  cache.changes |= change;
}

}  // namespace

bool roomDashboardStateUpdate(const char* entityId, const char* state,
                              JsonObject& attributes) {
  (void)attributes;
  if (matchesEntity(entityId, kOccupancyEntity)) {
    updateOccupancy(state);
  } else if (matchesEntity(entityId, kMotionBatteryEntity)) {
    updateBattery(state);
  } else if (matchesEntity(entityId, kCarbonDioxideEntity)) {
    updateMetric(cache.carbonDioxide, state, kCarbonDioxideChanged);
  } else if (matchesEntity(entityId, kTemperatureEntity)) {
    updateMetric(cache.temperature, state, kTemperatureChanged);
  } else if (matchesEntity(entityId, kHumidityEntity)) {
    updateMetric(cache.humidity, state, kHumidityChanged);
  } else if (matchesEntity(entityId, kWindowEntity)) {
    if (!stateIsUnavailable(state)) {
      cache.windowOpen = stateIsOn(state);
      cache.changes |= kWindowChanged;
    }
  } else if (matchesEntity(entityId, kTvPowerEntity)) {
    if (!stateIsUnavailable(state)) {
      cache.tvPowerOn = stateIsOn(state);
      cache.changes |= kTvPowerChanged;
    }
  } else if (matchesEntity(entityId, kCurrentPowerEntity)) {
    updateMetric(cache.currentPower, state, kCurrentPowerChanged);
  } else if (matchesEntity(entityId, kTodayEnergyEntity)) {
    updateMetric(cache.todayEnergy, state, kTodayEnergyChanged);
  } else if (matchesEntity(entityId, kMonthlyEnergyEntity)) {
    updateMetric(cache.monthlyEnergy, state, kMonthlyEnergyChanged);
  } else {
    return false;
  }

  Serial.printf("Dashboard state queued: %s = %s\n", entityId,
                state != nullptr ? state : "");
  return true;
}

void roomDashboardStateApply() {
  const uint16_t changes = cache.changes;
  if (changes == 0) {
    return;
  }
  cache.changes = 0;

  if (changes & kOccupancyChanged) {
    dashboardUiSetOccupancyState(cache.occupancy, cache.occupied);
  }
  if (changes & kBatteryChanged) {
    dashboardUiSetMotionBattery(cache.battery);
  }
  if (changes & kCarbonDioxideChanged) {
    dashboardUiSetMetric(DashboardMetric::CarbonDioxide,
                         cache.carbonDioxide, "ppm");
  }
  if (changes & kTemperatureChanged) {
    dashboardUiSetMetric(DashboardMetric::Temperature,
                         cache.temperature, "C");
  }
  if (changes & kHumidityChanged) {
    dashboardUiSetMetric(DashboardMetric::Humidity, cache.humidity, "%");
  }
  if (changes & kWindowChanged) {
    dashboardUiSetWindowState(cache.windowOpen);
  }
  if (changes & kTvPowerChanged) {
    dashboardUiSetTvPowerState(cache.tvPowerOn);
  }
  if (changes & kCurrentPowerChanged) {
    dashboardUiSetMetric(DashboardMetric::CurrentPower,
                         cache.currentPower, "W");
  }
  if (changes & kTodayEnergyChanged) {
    dashboardUiSetMetric(DashboardMetric::TodayEnergy,
                         cache.todayEnergy, "kWh");
  }
  if (changes & kMonthlyEnergyChanged) {
    dashboardUiSetMetric(DashboardMetric::MonthlyEnergy,
                         cache.monthlyEnergy, "kWh");
  }
}
