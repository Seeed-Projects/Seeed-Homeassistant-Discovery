"""
Seeed HA Discovery - BLE 开关实体
Seeed HA Discovery - BLE switch entities.

这个文件负责：
This file is responsible for:
1. 通过 GATT 连接控制 BLE 设备的开关
   Control BLE device switches via GATT connection
2. 监听状态通知更新开关状态
   Listen to state notifications and update switch state
3. 处理连接/断开事件
   Handle connect/disconnect events
"""
from __future__ import annotations

import asyncio
import logging
from typing import Any

from bleak import BleakClient, BleakError
from bleak.backends.device import BLEDevice

from homeassistant.components import bluetooth
from homeassistant.components.switch import SwitchEntity
from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant, callback
from homeassistant.helpers.device_registry import DeviceInfo
from homeassistant.helpers.entity_platform import AddEntitiesCallback

from .const import (
    DOMAIN,
    MANUFACTURER,
    CONF_BLE_ADDRESS,
    CONF_DEVICE_ID,
    CONF_MODEL,
    SEEED_CONTROL_SERVICE_UUID,
    SEEED_CONTROL_COMMAND_CHAR_UUID,
    SEEED_CONTROL_STATE_CHAR_UUID,
)

_LOGGER = logging.getLogger(__name__)

# 连接超时时间
CONNECTION_TIMEOUT = 10.0

# 重连间隔
RECONNECT_INTERVAL = 30.0


class SeeedBLEDeviceManager:
    """
    BLE 设备管理器
    BLE device manager.
    
    管理与 BLE 设备的 GATT 连接
    Manages GATT connections to BLE devices.
    """

    def __init__(
        self,
        hass: HomeAssistant,
        address: str,
        device_id: str,
    ) -> None:
        """
        初始化设备管理器
        Initialize device manager.
        """
        self._hass = hass
        self._address = address
        self._device_id = device_id
        self._client: BleakClient | None = None
        self._connected = False
        self._switches: list[SeeedBLESwitch] = []
        self._reconnect_task: asyncio.Task | None = None
        self._lock = asyncio.Lock()

    @property
    def connected(self) -> bool:
        """
        是否已连接
        Whether connected.
        """
        return self._connected and self._client is not None and self._client.is_connected

    def add_switch(self, switch: "SeeedBLESwitch") -> None:
        """
        添加开关实体
        Add switch entity.
        """
        self._switches.append(switch)

    async def async_connect(self) -> bool:
        """
        连接到 BLE 设备
        Connect to BLE device.
        """
        async with self._lock:
            if self._connected:
                return True

            try:
                # 正在连接 BLE 设备 | Connecting to BLE device
                _LOGGER.info("Connecting to BLE device: %s", self._address)

                # 获取 BLE 设备 | Get BLE device
                ble_device = bluetooth.async_ble_device_from_address(
                    self._hass, self._address
                )

                if ble_device is None:
                    # 找不到 BLE 设备 | Cannot find BLE device
                    _LOGGER.warning("Cannot find BLE device: %s", self._address)
                    return False

                # 创建 GATT 客户端
                self._client = BleakClient(
                    ble_device,
                    disconnected_callback=self._on_disconnect,
                )

                # 连接
                await asyncio.wait_for(
                    self._client.connect(),
                    timeout=CONNECTION_TIMEOUT,
                )

                self._connected = True
                # BLE 设备已连接 | BLE device connected
                _LOGGER.info("BLE device connected: %s", self._address)

                # 订阅状态通知 | Subscribe to state notifications
                await self._subscribe_notifications()

                # 读取初始状态 | Read initial state
                await self._read_initial_state()

                return True

            except asyncio.TimeoutError:
                # BLE 连接超时 | BLE connection timeout
                _LOGGER.warning("BLE connection timeout: %s", self._address)
                return False
            except BleakError as err:
                # BLE 连接失败 | BLE connection failed
                _LOGGER.warning("BLE connection failed: %s - %s", self._address, err)
                return False
            except Exception as err:
                # BLE 连接错误 | BLE connection error
                _LOGGER.exception("BLE connection error: %s", err)
                return False

    async def async_disconnect(self) -> None:
        """
        断开连接
        Disconnect.
        """
        if self._reconnect_task:
            self._reconnect_task.cancel()
            self._reconnect_task = None

        if self._client:
            try:
                await self._client.disconnect()
            except Exception:
                pass
            self._client = None

        self._connected = False

    async def async_send_command(self, switch_index: int, state: bool) -> bool:
        """
        发送开关命令
        Send switch command.
        
        命令格式: [switch_index][state]
        Command format: [switch_index][state]
        """
        if not self.connected:
            # 尝试重连 | Try to reconnect
            if not await self.async_connect():
                return False

        try:
            data = bytes([switch_index, 1 if state else 0])
            # 发送 BLE 命令 | Sending BLE command
            _LOGGER.debug("Sending BLE command: switch=%d, state=%s", switch_index, state)

            await self._client.write_gatt_char(
                SEEED_CONTROL_COMMAND_CHAR_UUID,
                data,
                response=False,
            )

            return True

        except BleakError as err:
            # BLE 命令发送失败 | BLE command send failed
            _LOGGER.warning("BLE command send failed: %s", err)
            self._connected = False
            self._schedule_reconnect()
            return False

    async def _subscribe_notifications(self) -> None:
        """
        订阅状态通知
        Subscribe to state notifications.
        """
        try:
            await self._client.start_notify(
                SEEED_CONTROL_STATE_CHAR_UUID,
                self._on_state_notification,
            )
            # 已订阅状态通知 | Subscribed to state notifications
            _LOGGER.debug("Subscribed to state notifications")
        except BleakError as err:
            # 订阅通知失败 | Failed to subscribe to notifications
            _LOGGER.warning("Failed to subscribe to notifications: %s", err)

    async def _read_initial_state(self) -> None:
        """
        读取初始状态
        Read initial state.
        """
        try:
            data = await self._client.read_gatt_char(SEEED_CONTROL_STATE_CHAR_UUID)
            self._parse_state_data(data)
        except BleakError as err:
            # 读取初始状态失败 | Failed to read initial state
            _LOGGER.warning("Failed to read initial state: %s", err)

    def _on_state_notification(self, sender: int, data: bytearray) -> None:
        """
        处理状态通知
        Handle state notification.
        
        状态格式: [switch_count][sw0_state][sw1_state]...
        State format: [switch_count][sw0_state][sw1_state]...
        """
        # 收到状态通知 | Received state notification
        _LOGGER.debug("Received state notification: %s", data.hex())
        self._parse_state_data(data)

    def _parse_state_data(self, data: bytes | bytearray) -> None:
        """
        解析状态数据
        Parse state data.
        """
        if len(data) < 1:
            return

        switch_count = data[0]
        for i in range(min(switch_count, len(self._switches))):
            if i + 1 < len(data):
                state = data[i + 1] != 0
                self._switches[i].update_state(state)

    def _on_disconnect(self, client: BleakClient) -> None:
        """
        处理断开连接
        Handle disconnection.
        """
        # BLE 设备断开连接 | BLE device disconnected
        _LOGGER.info("BLE device disconnected: %s", self._address)
        self._connected = False

        # 更新所有开关为不可用 | Mark all switches as unavailable
        for switch in self._switches:
            switch.set_unavailable()

        # 安排重连 | Schedule reconnect
        self._schedule_reconnect()

    def _schedule_reconnect(self) -> None:
        """
        安排重连
        Schedule reconnection.
        """
        if self._reconnect_task is None or self._reconnect_task.done():
            self._reconnect_task = asyncio.create_task(self._reconnect_loop())

    async def _reconnect_loop(self) -> None:
        """
        重连循环
        Reconnection loop.
        """
        while not self._connected:
            await asyncio.sleep(RECONNECT_INTERVAL)
            # 尝试重连 BLE 设备 | Trying to reconnect to BLE device
            _LOGGER.info("Trying to reconnect to BLE device: %s", self._address)
            if await self.async_connect():
                # 更新所有开关为可用 | Mark all switches as available
                for switch in self._switches:
                    switch.set_available()
                break


class SeeedBLESwitch(SwitchEntity):
    """
    Seeed BLE 开关实体
    通过 GATT 控制 BLE 设备的开关
    """

    _attr_has_entity_name = True

    def __init__(
        self,
        entry: ConfigEntry,
        device_id: str,
        device_name: str,
        model: str,
        switch_index: int,
        switch_name: str,
        manager: SeeedBLEDeviceManager,
    ) -> None:
        """
        初始化 BLE 开关
        Initialize BLE switch.
        """
        self._entry = entry
        self._device_id = device_id
        self._device_name = device_name
        self._model = model
        self._switch_index = switch_index
        self._manager = manager

        # 设置实体属性
        self._attr_name = switch_name
        self._attr_unique_id = f"{device_id}_switch_{switch_index}"
        self._attr_is_on = False
        self._attr_available = False

        # 将自己添加到管理器
        manager.add_switch(self)

    @property
    def device_info(self) -> DeviceInfo:
        """
        返回设备信息
        Return device info.
        """
        return DeviceInfo(
            identifiers={(DOMAIN, self._device_id)},
            name=self._device_name,
            manufacturer=MANUFACTURER,
            model=self._model,
        )

    async def async_turn_on(self, **kwargs: Any) -> None:
        """
        打开开关
        Turn on the switch.
        """
        _LOGGER.debug("BLE switch %s: turn_on", self._attr_unique_id)
        if await self._manager.async_send_command(self._switch_index, True):
            self._attr_is_on = True
            self.async_write_ha_state()

    async def async_turn_off(self, **kwargs: Any) -> None:
        """
        关闭开关
        Turn off the switch.
        """
        _LOGGER.debug("BLE switch %s: turn_off", self._attr_unique_id)
        if await self._manager.async_send_command(self._switch_index, False):
            self._attr_is_on = False
            self.async_write_ha_state()

    @callback
    def update_state(self, state: bool) -> None:
        """
        更新开关状态（从设备通知）
        Update switch state (from device notification).
        """
        self._attr_is_on = state
        self._attr_available = True
        self.async_write_ha_state()
        # BLE 开关状态更新 | BLE switch state updated
        _LOGGER.debug("BLE switch %s state updated: %s", self._attr_unique_id, state)

    @callback
    def set_unavailable(self) -> None:
        """
        设置为不可用
        Set as unavailable.
        """
        self._attr_available = False
        self.async_write_ha_state()

    @callback
    def set_available(self) -> None:
        """
        设置为可用
        Set as available.
        """
        self._attr_available = True
        self.async_write_ha_state()


async def async_setup_ble_switches(
    hass: HomeAssistant,
    entry: ConfigEntry,
    async_add_entities: AddEntitiesCallback,
) -> SeeedBLEDeviceManager | None:
    """
    设置 BLE 开关实体
    
    返回设备管理器，以便后续管理连接
    """
    ble_address = entry.data[CONF_BLE_ADDRESS]
    device_id = entry.data.get(CONF_DEVICE_ID, f"ble_{ble_address.replace(':', '_').lower()}")
    device_name = entry.title
    model = entry.data.get(CONF_MODEL, "XIAO BLE")

    # 设置 BLE 开关 | Setting up BLE switches
    _LOGGER.info("="*50)
    _LOGGER.info("Setting up BLE switches: %s (%s)", device_name, ble_address)
    _LOGGER.info("Entry data: %s", entry.data)
    _LOGGER.info("="*50)

    # 创建设备管理器 | Create device manager
    manager = SeeedBLEDeviceManager(hass, ble_address, device_id)

    # 从设备数据中获取开关配置 | Get switch configs from device data
    # 通过 binary 传感器的数量推测开关数量
    # Infer switch count from binary sensor count
    switch_configs = entry.data.get("switch_configs", [])
    
    if not switch_configs:
        # 默认配置：单个 LED 开关 | Default config: single LED switch
        switch_configs = [{"index": 0, "name": "LED", "id": "led"}]
    
    # 创建 BLE 开关 | Creating BLE switches
    _LOGGER.info("Creating %d BLE switches", len(switch_configs))
    
    # 创建所有开关
    switches = []
    for config in switch_configs:
        switch = SeeedBLESwitch(
            entry=entry,
            device_id=device_id,
            device_name=device_name,
            model=model,
            switch_index=config["index"],
            switch_name=config["name"],
            manager=manager,
        )
        switches.append(switch)
        manager.add_switch(switch)
    
    async_add_entities(switches)

    # 尝试连接
    asyncio.create_task(manager.async_connect())

    return manager

