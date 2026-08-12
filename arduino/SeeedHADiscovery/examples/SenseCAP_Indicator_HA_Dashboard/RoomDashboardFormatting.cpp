#include "RoomDashboardFormatting.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

bool formatDashboardNumber(char* destination, size_t destinationLength,
                           const char* state, uint8_t decimals) {
  if (destination == nullptr || destinationLength == 0 || state == nullptr ||
      state[0] == '\0') {
    return false;
  }

  char* end = nullptr;
  const float value = strtof(state, &end);
  if (end == state || end == nullptr || end[0] != '\0' || !isfinite(value)) {
    return false;
  }

  const int written = snprintf(destination, destinationLength, "%.*f",
                               static_cast<int>(decimals), value);
  return written >= 0 && static_cast<size_t>(written) < destinationLength;
}
