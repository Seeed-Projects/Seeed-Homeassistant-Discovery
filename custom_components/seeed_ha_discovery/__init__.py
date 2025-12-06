"""
Seeed HA Discovery - 主入口文件
Seeed HA Discovery - Main entry file.

这个文件是 Home Assistant 集成的主入口，负责：
This file is the main entry point for the Home Assistant integration, responsible for:
1. 初始化集成 - 当用户添加设备时调用
   Initialize integration - called when user adds a device
2. 区分 WiFi 和 BLE 设备，使用不同的连接方式
   Distinguish between WiFi and BLE devices, use different connection methods
3. WiFi 设备：建立与 ESP32 设备的 WebSocket 连接
   WiFi devices: Establish WebSocket connection with ESP32 devices
4. BLE 设备：通过蓝牙被动监听广播数据
   BLE devices: Passively listen to Bluetooth broadcast data
5. 加载传感器平台
   Load sensor platforms
6. 处理集成的卸载
   Handle integration unloading

工作流程 | Workflow:
1. 用户通过配置流程添加设备（手动输入IP/自动发现WiFi/自动发现BLE）
   User adds device via config flow (manual IP/WiFi auto-discovery/BLE auto-discovery)
2. async_setup_entry() 被调用，根据连接类型创建设备连接
   async_setup_entry() is called, creates device connection based on connection type
3. 加载 sensor 平台，创建传感器实体
   Load sensor platform, create sensor entities
4. WiFi: 保持 WebSocket 连接，实时接收传感器数据
   WiFi: Maintain WebSocket connection, receive sensor data in real-time
5. BLE: 被动监听蓝牙广播，自动更新传感器数据
   BLE: Passively listen to Bluetooth broadcasts, auto-update sensor data
"""
from __future__ import annotations

import logging
from typing import Any

from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant
from homeassistant.exceptions import ConfigEntryNotReady

from .const import (
    DOMAIN,
    PLATFORMS,
    CONF_HOST,
    CONF_PORT,
    CONF_CONNECTION_TYPE,
    CONF_BLE_ADDRESS,
    CONF_SUBSCRIBED_ENTITIES,
    CONNECTION_TYPE_WIFI,
    CONNECTION_TYPE_BLE,
    DEFAULT_WS_PORT,
)
from .coordinator import SeeedHACoordinator
from .device import SeeedHADevice

# 创建日志记录器
# Create logger for this module
_LOGGER = logging.getLogger(__name__)


async def async_setup_entry(hass: HomeAssistant, entry: ConfigEntry) -> bool:
    """
    设置配置入口
    Set up Seeed HA Discovery from a config entry.

    当用户成功添加一个设备后，这个函数会被调用。
    This function is called when a user successfully adds a device.

    参数 | Args:
        hass: Home Assistant 实例 | Home Assistant instance
        entry: 配置入口，包含设备信息 | Config entry containing device info

    返回 | Returns:
        bool: 设置是否成功 | Whether setup was successful
    """
    # 确保存储字典存在
    # Ensure the storage dictionary exists
    hass.data.setdefault(DOMAIN, {})
    hass.data[DOMAIN][entry.entry_id] = {}

    # 获取连接类型，默认为 WiFi（兼容旧配置）
    connection_type = entry.data.get(CONF_CONNECTION_TYPE, CONNECTION_TYPE_WIFI)

    if connection_type == CONNECTION_TYPE_BLE:
        # BLE 设备设置
        return await _async_setup_ble_entry(hass, entry)
    else:
        # WiFi 设备设置
        return await _async_setup_wifi_entry(hass, entry)


async def _async_setup_wifi_entry(hass: HomeAssistant, entry: ConfigEntry) -> bool:
    """
    设置 WiFi 设备
    Set up a WiFi device.
    """
    # 从配置中获取设备地址和端口
    host = entry.data[CONF_HOST]
    port = entry.data.get(CONF_PORT, DEFAULT_WS_PORT)

    # 正在设置 WiFi 设备 | Setting up WiFi device
    _LOGGER.info("Setting up Seeed HA WiFi device: %s:%s", host, port)

    # 创建设备实例 - 负责与 ESP32 通信
    device = SeeedHADevice(hass, host, port, entry)

    # 创建协调器 - 管理数据更新和实体状态
    coordinator = SeeedHACoordinator(hass, device, entry)

    # 尝试连接到设备
    try:
        await coordinator.async_connect()
        # 连接成功 | Connection successful
        _LOGGER.info("Successfully connected to WiFi device %s", host)
    except Exception as err:
        # 连接失败 | Connection failed
        _LOGGER.error("Failed to connect to Seeed HA WiFi device at %s: %s", host, err)
        raise ConfigEntryNotReady(f"Cannot connect to {host}") from err

    # 保存设备和协调器引用
    hass.data[DOMAIN][entry.entry_id] = {
        "device": device,
        "coordinator": coordinator,
        "connection_type": CONNECTION_TYPE_WIFI,
    }

    # 设置 HA 实体订阅（如果配置了）
    # Set up HA entity subscription (if configured)
    subscribed_entities = entry.options.get(CONF_SUBSCRIBED_ENTITIES, [])
    if subscribed_entities:
        await device.async_setup_entity_subscription(subscribed_entities)
        _LOGGER.info("Subscribed to %d HA entities for device %s", len(subscribed_entities), host)

    # 加载传感器平台
    await hass.config_entries.async_forward_entry_setups(entry, PLATFORMS)

    # 注册配置更新监听器
    entry.async_on_unload(entry.add_update_listener(async_update_options))

    return True


async def _async_setup_ble_entry(hass: HomeAssistant, entry: ConfigEntry) -> bool:
    """
    设置 BLE 设备
    Set up a BLE device.

    BLE 设备支持两种模式：
    1. 被动监听模式 - 传感器数据通过蓝牙广播接收
    2. 主动连接模式 - 通过 GATT 控制设备（如开关）
    """
    from .const import CONF_BLE_CONTROL, CONF_BLE_SUBSCRIBED_ENTITIES

    ble_address = entry.data[CONF_BLE_ADDRESS]
    ble_control = entry.data.get(CONF_BLE_CONTROL, False)
    ble_subscribed = entry.data.get(CONF_BLE_SUBSCRIBED_ENTITIES, {})

    # 正在设置 BLE 设备 | Setting up BLE device
    _LOGGER.info(
        "Setting up Seeed HA BLE device: %s (control=%s, subscribed=%d)",
        ble_address, ble_control, len(ble_subscribed)
    )

    # 加载平台（BLE 设备不支持摄像头）
    # Load platforms (BLE devices don't support camera)
    # 如果有控制服务或订阅实体，需要加载 switch 平台来创建 BLE 管理器
    # If control service or subscribed entities, need switch platform for BLE manager
    platforms = ["sensor"]
    if ble_control or ble_subscribed:
        platforms.append("switch")
        _LOGGER.info("Loading switch platform for BLE manager (control=%s, subscribed=%d)", 
                     ble_control, len(ble_subscribed))

    # 保存配置和已加载的平台列表（用于卸载时）
    # Save config and list of loaded platforms (for unloading)
    hass.data[DOMAIN][entry.entry_id] = {
        "ble_address": ble_address,
        "connection_type": CONNECTION_TYPE_BLE,
        "ble_control": ble_control or bool(ble_subscribed),
        "loaded_platforms": platforms,  # Record actually loaded platforms | 记录实际加载的平台
    }

    await hass.config_entries.async_forward_entry_setups(entry, platforms)

    # 注册配置更新监听器
    entry.async_on_unload(entry.add_update_listener(async_update_options))

    # BLE 设备设置完成 | BLE device setup complete
    _LOGGER.info("BLE device setup complete: %s", ble_address)
    return True


async def async_unload_entry(hass: HomeAssistant, entry: ConfigEntry) -> bool:
    """
    卸载配置入口
    Unload a config entry.

    当用户删除设备或重新加载集成时调用。
    Called when user removes device or reloads the integration.

    参数 | Args:
        hass: Home Assistant 实例
        entry: 要卸载的配置入口

    返回 | Returns:
        bool: 卸载是否成功
    """
    from .const import CONF_BLE_CONTROL, CONF_BLE_SUBSCRIBED_ENTITIES

    # 正在卸载设备 | Unloading device
    _LOGGER.info("Unloading Seeed HA device")

    # 获取连接类型
    data = hass.data[DOMAIN].get(entry.entry_id, {})
    connection_type = data.get("connection_type", CONNECTION_TYPE_WIFI)
    ble_control = data.get("ble_control", False)
    ble_subscribed = entry.data.get(CONF_BLE_SUBSCRIBED_ENTITIES, {})

    # 确定要卸载的平台 - 使用运行时记录的已加载平台，而不是当前配置
    # Determine platforms to unload - use runtime record, not current config
    if connection_type == CONNECTION_TYPE_BLE:
        # 使用运行时记录的平台列表
        # Use the platforms that were actually loaded at runtime
        platforms_to_unload = data.get("loaded_platforms", ["sensor"])
        _LOGGER.info("BLE device unloading platforms: %s", platforms_to_unload)
    else:
        platforms_to_unload = PLATFORMS

    # 卸载所有平台
    try:
        unload_ok = await hass.config_entries.async_unload_platforms(entry, platforms_to_unload)
    except ValueError as err:
        # 平台可能从未加载，记录警告但继续
        # Platform might never have been loaded, log warning but continue
        _LOGGER.warning("Error unloading platforms (may not have been loaded): %s", err)
        unload_ok = True  # Allow cleanup to continue | 允许继续清理

    if unload_ok:
        # 获取并清理设备数据
        data = hass.data[DOMAIN].pop(entry.entry_id, {})

        # 如果是 WiFi 设备，断开 WebSocket 连接
        if connection_type == CONNECTION_TYPE_WIFI and "coordinator" in data:
            coordinator: SeeedHACoordinator = data["coordinator"]
            await coordinator.async_disconnect()

        # 如果是 BLE 设备且有控制功能，断开 GATT 连接
        if connection_type == CONNECTION_TYPE_BLE and "ble_manager" in data:
            manager = data["ble_manager"]
            await manager.async_disconnect()

        # 卸载完成 | Unload complete
        _LOGGER.info("Device unloaded successfully")

    return unload_ok


async def async_update_options(hass: HomeAssistant, entry: ConfigEntry) -> None:
    """
    处理配置选项更新
    Handle options update.

    当用户修改设备配置时调用。
    Called when user modifies device config.
    """
    from .const import CONF_BLE_SUBSCRIBED_ENTITIES

    # 配置已更新 | Config updated
    _LOGGER.info("Config options updated for %s", entry.title)

    # 获取设备数据 | Get device data
    data = hass.data[DOMAIN].get(entry.entry_id, {})
    connection_type = data.get("connection_type", CONNECTION_TYPE_WIFI)

    # 如果是 WiFi 设备，更新实体订阅 | If WiFi device, update entity subscription
    if connection_type == CONNECTION_TYPE_WIFI and "device" in data:
        device: SeeedHADevice = data["device"]
        subscribed_entities = entry.options.get(CONF_SUBSCRIBED_ENTITIES, [])
        await device.async_setup_entity_subscription(subscribed_entities)
        _LOGGER.info("Updated entity subscription: %d entities", len(subscribed_entities))

    # BLE 设备的重载由 config_flow 的 delayed_reload 处理，这里不重复重载
    # BLE device reload is handled by config_flow's delayed_reload, don't reload here
    elif connection_type == CONNECTION_TYPE_BLE:
        ble_subscribed = entry.data.get(CONF_BLE_SUBSCRIBED_ENTITIES, {})
        _LOGGER.info("BLE config changed (subscribed=%d), reload will be triggered by config_flow", len(ble_subscribed))
        # 不在这里重载，由 config_flow 的 delayed_reload 处理
        # Don't reload here, let config_flow's delayed_reload handle it
