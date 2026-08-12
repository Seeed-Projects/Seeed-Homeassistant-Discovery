#pragma once

#include <stddef.h>
#include <stdint.h>

// Formats a numeric HA state with a fixed number of decimal places.
// 将 HA 数值状态格式化为固定的小数位数。
bool formatDashboardNumber(char* destination, size_t destinationLength,
                           const char* state, uint8_t decimals);
