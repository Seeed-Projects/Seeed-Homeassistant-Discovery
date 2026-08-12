#pragma once

#include <ArduinoJson.h>

// Stores one HA update for the next LVGL refresh cycle.
// 缓存一条 HA 更新，等待下一次 LVGL 刷新周期统一应用。
bool roomDashboardStateUpdate(const char* entityId, const char* state,
                              JsonObject& attributes);

// Applies all pending entity changes to the dashboard widgets.
// 将所有待处理的实体变化应用到看板控件。
void roomDashboardStateApply();
