#include <assert.h>
#include <string.h>

#include "RoomDashboardFormatting.h"

int main() {
  char output[24] = {};

  assert(formatDashboardNumber(output, sizeof(output), "26.11887", 1));
  assert(strcmp(output, "26.1") == 0);

  assert(formatDashboardNumber(output, sizeof(output), "49.22255", 1));
  assert(strcmp(output, "49.2") == 0);

  assert(formatDashboardNumber(output, sizeof(output), "26.16", 1));
  assert(strcmp(output, "26.2") == 0);

  assert(!formatDashboardNumber(output, sizeof(output), "unavailable", 1));
  assert(!formatDashboardNumber(output, sizeof(output), "26.1 C", 1));
  return 0;
}
