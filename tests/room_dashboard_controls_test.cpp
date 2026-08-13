#include <assert.h>
#include <string.h>

#include "RoomDashboardConfig.h"

int main() {
  assert(strcmp(kLeftSwitchEntity,
                "switch.xiaomi_2wpro2_37c3_left_switch_service_2") == 0);
  assert(strcmp(kRightSwitchEntity,
                "switch.xiaomi_2wpro2_37c3_right_switch_service_2") == 0);
  assert(strcmp(kReservedAirConditionerEntity,
                "switch.indicator_switch1_1") == 0);
  assert(kLeaveRoomEntityCount == 6);

  bool airConditionerIncluded = false;
  for (size_t index = 0; index < kLeaveRoomEntityCount; ++index) {
    if (strcmp(kLeaveRoomEntities[index],
               kReservedAirConditionerEntity) == 0) {
      airConditionerIncluded = true;
    }
  }
  assert(airConditionerIncluded);
  return 0;
}
