#pragma once

// Room and network names shown by the reusable dashboard template.
// 可复用看板模板显示的会议室名称与配网热点名称。
constexpr const char* kDashboardRoomName = "ROOM 106";
constexpr const char* kDashboardProvisioningAp = "SenseCAP_Indicator_AP";

// Home Assistant entity slots for this meeting-room deployment.
// 本会议室部署中各个 Home Assistant 实体对应的显示槽位。
constexpr const char* kOccupancyEntity =
    "sensor.xiaomi_03_1163_occupancy_sensor_2";
constexpr const char* kMotionBatteryEntity =
    "sensor.xiaomi_03_1163_battery_level_2";
constexpr const char* kCarbonDioxideEntity =
    "sensor.scd41_air_quality_monitor_carbon_dioxide";
constexpr const char* kTemperatureEntity =
    "sensor.scd41_air_quality_monitor_temperature";
constexpr const char* kHumidityEntity =
    "sensor.scd41_air_quality_monitor_humidity";
constexpr const char* kWindowEntity =
    "cover.liyan_liyan_4d07_window_opener_2";
constexpr const char* kTvPowerEntity =
    "switch.cuco_v3_3244_switch_2";
constexpr const char* kCurrentPowerEntity =
    "sensor.cuco_v3_3244_electric_power_2";
constexpr const char* kTodayEnergyEntity =
    "sensor.cuco_v3_3244_power_cost_today_2";
constexpr const char* kMonthlyEnergyEntity =
    "sensor.cuco_v3_3244_power_cost_month_2";
