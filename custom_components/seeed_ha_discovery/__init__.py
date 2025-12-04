"""
Seeed HA Discovery - 主入口文件
The Seeed HA Discovery integration.

这个文件是 Home Assistant 集成的主入口，负责：
1. 初始化集成 - 当用户添加设备时调用
2. 区分 WiFi 和 BLE 设备，使用不同的连接方式
3. WiFi 设备：建立与 ESP32 设备的 WebSocket 连接
4. BLE 设备：通过蓝牙被动监听广播数据
5. 加载传感器平台
6. 处理集成的卸载

工作流程：
1. 用户通过配置流程添加设备（手动输入IP/自动发现WiFi/自动发现BLE）
2. async_setup_entry() 被调用，根据连接类型创建设备连接
3. 加载 sensor 平台，创建传感器实体
4. WiFi: 保持 WebSocket 连接，实时接收传感器数据
5. BLE: 被动监听蓝牙广播，自动更新传感器数据
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

    _LOGGER.info("正在设置 Seeed HA WiFi 设备: %s:%s", host, port)
    _LOGGER.info("Setting up Seeed HA WiFi device: %s:%s", host, port)

    # 创建设备实例 - 负责与 ESP32 通信
    device = SeeedHADevice(hass, host, port, entry)

    # 创建协调器 - 管理数据更新和实体状态
    coordinator = SeeedHACoordinator(hass, device, entry)

    # 尝试连接到设备
    try:
        await coordinator.async_connect()
        _LOGGER.info("成功连接到 WiFi 设备 %s", host)
        _LOGGER.info("Successfully connected to WiFi device %s", host)
    except Exception as err:
        _LOGGER.error("无法连接到 Seeed HA WiFi 设备 %s: %s", host, err)
        _LOGGER.error("Failed to connect to Seeed HA WiFi device at %s: %s", host, err)
        raise ConfigEntryNotReady(f"无法连接到 {host}") from err

    # 保存设备和协调器引用
    hass.data[DOMAIN][entry.entry_id] = {
        "device": device,
        "coordinator": coordinator,
        "connection_type": CONNECTION_TYPE_WIFI,
    }

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
    from .const import CONF_BLE_CONTROL

    ble_address = entry.data[CONF_BLE_ADDRESS]
    ble_control = entry.data.get(CONF_BLE_CONTROL, False)

    _LOGGER.info("正在设置 Seeed HA BLE 设备: %s (控制=%s)", ble_address, ble_control)
    _LOGGER.info("Setting up Seeed HA BLE device: %s (control=%s)", ble_address, ble_control)

    # 保存配置
    hass.data[DOMAIN][entry.entry_id] = {
        "ble_address": ble_address,
        "connection_type": CONNECTION_TYPE_BLE,
        "ble_control": ble_control,
    }

    # 加载平台
    platforms = ["sensor"]
    if ble_control:
        platforms.append("switch")

    await hass.config_entries.async_forward_entry_setups(entry, platforms)

    # 注册配置更新监听器
    entry.async_on_unload(entry.add_update_listener(async_update_options))

    _LOGGER.info("BLE 设备设置完成: %s", ble_address)
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
    from .const import CONF_BLE_CONTROL

    _LOGGER.info("正在卸载 Seeed HA 设备")
    _LOGGER.info("Unloading Seeed HA device")

    # 获取连接类型
    data = hass.data[DOMAIN].get(entry.entry_id, {})
    connection_type = data.get("connection_type", CONNECTION_TYPE_WIFI)
    ble_control = data.get("ble_control", False)

    # 确定要卸载的平台
    if connection_type == CONNECTION_TYPE_BLE:
        platforms_to_unload = ["sensor"]
        if ble_control:
            platforms_to_unload.append("switch")
    else:
        platforms_to_unload = PLATFORMS

    # 卸载所有平台
    unload_ok = await hass.config_entries.async_unload_platforms(entry, platforms_to_unload)

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

        _LOGGER.info("设备卸载完成")
        _LOGGER.info("Device unloaded successfully")

    return unload_ok


async def async_update_options(hass: HomeAssistant, entry: ConfigEntry) -> None:
    """
    处理配置选项更新
    Handle options update.

    当用户修改设备配置时调用，会重新加载集成。
    Called when user modifies device config, will reload the integration.
    """
    _LOGGER.info("配置已更新，正在重新加载")
    _LOGGER.info("Config updated, reloading")
    await hass.config_entries.async_reload(entry.entry_id)
