#pragma once

#include <stdint.h>

enum class DashboardOccupancyState : uint8_t {
  Unknown,
  Vacant,
  Occupied,
};

// Converts common HA presence states into one dashboard occupancy state.
// 将常见的 HA 人员存在状态转换成统一的看板人员状态。
DashboardOccupancyState parseDashboardOccupancy(const char* state);
