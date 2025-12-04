"""
Seeed HA Discovery - BLE 开关实体
BLE switch entities for Seeed HA Discovery.

这个文件负责：
1. 通过 GATT 连接控制 BLE 设备的开关
2. 监听状态通知更新开关状态
3. 处理连接/断开事件
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
    管理与 BLE 设备的 GATT 连接
    """

    def __init__(
        self,
        hass: HomeAssistant,
        address: str,
        device_id: str,
    ) -> None:
        """初始化设备管理器"""
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
        """是否已连接"""
        return self._connected and self._client is not None and self._client.is_connected

    def add_switch(self, switch: "SeeedBLESwitch") -> None:
        """添加开关实体"""
        self._switches.append(switch)

    async def async_connect(self) -> bool:
        """
        连接到 BLE 设备
        """
        async with self._lock:
            if self._connected:
                return True

            try:
                _LOGGER.info("正在连接 BLE 设备: %s", self._address)

                # 获取 BLE 设备
                ble_device = bluetooth.async_ble_device_from_address(
                    self._hass, self._address
                )

                if ble_device is None:
                    _LOGGER.warning("找不到 BLE 设备: %s", self._address)
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
                _LOGGER.info("BLE 设备已连接: %s", self._address)

                # 订阅状态通知
                await self._subscribe_notifications()

                # 读取初始状态
                await self._read_initial_state()

                return True

            except asyncio.TimeoutError:
                _LOGGER.warning("BLE 连接超时: %s", self._address)
                return False
            except BleakError as err:
                _LOGGER.warning("BLE 连接失败: %s - %s", self._address, err)
                return False
            except Exception as err:
                _LOGGER.exception("BLE 连接错误: %s", err)
                return False

    async def async_disconnect(self) -> None:
        """断开连接"""
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
        
        命令格式: [switch_index][state]
        """
        if not self.connected:
            # 尝试重连
            if not await self.async_connect():
                return False

        try:
            data = bytes([switch_index, 1 if state else 0])
            _LOGGER.debug("发送 BLE 命令: switch=%d, state=%s", switch_index, state)

            await self._client.write_gatt_char(
                SEEED_CONTROL_COMMAND_CHAR_UUID,
                data,
                response=False,
            )

            return True

        except BleakError as err:
            _LOGGER.warning("BLE 命令发送失败: %s", err)
            self._connected = False
            self._schedule_reconnect()
            return False

    async def _subscribe_notifications(self) -> None:
        """订阅状态通知"""
        try:
            await self._client.start_notify(
                SEEED_CONTROL_STATE_CHAR_UUID,
                self._on_state_notification,
            )
            _LOGGER.debug("已订阅状态通知")
        except BleakError as err:
            _LOGGER.warning("订阅通知失败: %s", err)

    async def _read_initial_state(self) -> None:
        """读取初始状态"""
        try:
            data = await self._client.read_gatt_char(SEEED_CONTROL_STATE_CHAR_UUID)
            self._parse_state_data(data)
        except BleakError as err:
            _LOGGER.warning("读取初始状态失败: %s", err)

    def _on_state_notification(self, sender: int, data: bytearray) -> None:
        """
        处理状态通知
        
        状态格式: [switch_count][sw0_state][sw1_state]...
        """
        _LOGGER.debug("收到状态通知: %s", data.hex())
        self._parse_state_data(data)

    def _parse_state_data(self, data: bytes | bytearray) -> None:
        """解析状态数据"""
        if len(data) < 1:
            return

        switch_count = data[0]
        for i in range(min(switch_count, len(self._switches))):
            if i + 1 < len(data):
                state = data[i + 1] != 0
                self._switches[i].update_state(state)

    def _on_disconnect(self, client: BleakClient) -> None:
        """处理断开连接"""
        _LOGGER.info("BLE 设备断开连接: %s", self._address)
        self._connected = False

        # 更新所有开关为不可用
        for switch in self._switches:
            switch.set_unavailable()

        # 安排重连
        self._schedule_reconnect()

    def _schedule_reconnect(self) -> None:
        """安排重连"""
        if self._reconnect_task is None or self._reconnect_task.done():
            self._reconnect_task = asyncio.create_task(self._reconnect_loop())

    async def _reconnect_loop(self) -> None:
        """重连循环"""
        while not self._connected:
            await asyncio.sleep(RECONNECT_INTERVAL)
            _LOGGER.info("尝试重连 BLE 设备: %s", self._address)
            if await self.async_connect():
                # 更新所有开关为可用
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
        """初始化 BLE 开关"""
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
        """返回设备信息"""
        return DeviceInfo(
            identifiers={(DOMAIN, self._device_id)},
            name=self._device_name,
            manufacturer=MANUFACTURER,
            model=self._model,
        )

    async def async_turn_on(self, **kwargs: Any) -> None:
        """打开开关"""
        _LOGGER.debug("BLE 开关 %s: turn_on", self._attr_unique_id)
        if await self._manager.async_send_command(self._switch_index, True):
            self._attr_is_on = True
            self.async_write_ha_state()

    async def async_turn_off(self, **kwargs: Any) -> None:
        """关闭开关"""
        _LOGGER.debug("BLE 开关 %s: turn_off", self._attr_unique_id)
        if await self._manager.async_send_command(self._switch_index, False):
            self._attr_is_on = False
            self.async_write_ha_state()

    @callback
    def update_state(self, state: bool) -> None:
        """更新开关状态（从设备通知）"""
        self._attr_is_on = state
        self._attr_available = True
        self.async_write_ha_state()
        _LOGGER.debug("BLE 开关 %s 状态更新: %s", self._attr_unique_id, state)

    @callback
    def set_unavailable(self) -> None:
        """设置为不可用"""
        self._attr_available = False
        self.async_write_ha_state()

    @callback
    def set_available(self) -> None:
        """设置为可用"""
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

    _LOGGER.info("="*50)
    _LOGGER.info("设置 BLE 开关: %s (%s)", device_name, ble_address)
    _LOGGER.info("Entry data: %s", entry.data)
    _LOGGER.info("="*50)

    # 创建设备管理器
    manager = SeeedBLEDeviceManager(hass, ble_address, device_id)

    # 从设备数据中获取开关配置
    # 通过 binary 传感器的数量推测开关数量
    switch_configs = entry.data.get("switch_configs", [])
    
    if not switch_configs:
        # 默认配置：单个 LED 开关
        switch_configs = [{"index": 0, "name": "LED", "id": "led"}]
    
    _LOGGER.info("创建 %d 个 BLE 开关", len(switch_configs))
    
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

