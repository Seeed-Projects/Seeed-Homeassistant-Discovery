#include <assert.h>

#include "RoomDashboardOccupancy.h"

int main() {
  assert(parseDashboardOccupancy("has one") ==
         DashboardOccupancyState::Occupied);
  assert(parseDashboardOccupancy("has_one") ==
         DashboardOccupancyState::Occupied);
  assert(parseDashboardOccupancy("occupied") ==
         DashboardOccupancyState::Occupied);
  assert(parseDashboardOccupancy("no one") ==
         DashboardOccupancyState::Vacant);
  assert(parseDashboardOccupancy("no_one") ==
         DashboardOccupancyState::Vacant);
  assert(parseDashboardOccupancy("unavailable") ==
         DashboardOccupancyState::Unknown);
  assert(parseDashboardOccupancy("unexpected") ==
         DashboardOccupancyState::Unknown);
  return 0;
}
