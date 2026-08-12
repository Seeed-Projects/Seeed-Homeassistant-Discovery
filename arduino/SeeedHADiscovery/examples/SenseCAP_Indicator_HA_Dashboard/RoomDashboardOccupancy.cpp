#include "RoomDashboardOccupancy.h"

#include <strings.h>

namespace {

bool equalsIgnoreCase(const char* left, const char* right) {
  return left != nullptr && right != nullptr && strcasecmp(left, right) == 0;
}

}  // namespace

DashboardOccupancyState parseDashboardOccupancy(const char* state) {
  if (state == nullptr || state[0] == '\0' ||
      equalsIgnoreCase(state, "unknown") ||
      equalsIgnoreCase(state, "unavailable") ||
      equalsIgnoreCase(state, "none")) {
    return DashboardOccupancyState::Unknown;
  }

  if (equalsIgnoreCase(state, "has one") ||
      equalsIgnoreCase(state, "has_one") ||
      equalsIgnoreCase(state, "occupied") ||
      equalsIgnoreCase(state, "detected") ||
      equalsIgnoreCase(state, "present") ||
      equalsIgnoreCase(state, "someone") || equalsIgnoreCase(state, "home") ||
      equalsIgnoreCase(state, "on") || equalsIgnoreCase(state, "true") ||
      equalsIgnoreCase(state, "1")) {
    return DashboardOccupancyState::Occupied;
  }

  if (equalsIgnoreCase(state, "no one") ||
      equalsIgnoreCase(state, "no_one") ||
      equalsIgnoreCase(state, "vacant") ||
      equalsIgnoreCase(state, "clear") ||
      equalsIgnoreCase(state, "nobody") || equalsIgnoreCase(state, "away") ||
      equalsIgnoreCase(state, "off") || equalsIgnoreCase(state, "false") ||
      equalsIgnoreCase(state, "0")) {
    return DashboardOccupancyState::Vacant;
  }

  return DashboardOccupancyState::Unknown;
}
