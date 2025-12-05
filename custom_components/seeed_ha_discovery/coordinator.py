"""
Seeed HA Discovery - 数据协调器
Seeed HA Discovery - Data Coordinator.

这个模块是 Home Assistant 和设备之间的桥梁：
This module is the bridge between Home Assistant and devices:
1. 管理与设备的连接
   Manage connections with devices
2. 接收设备的状态更新
   Receive device state updates
3. 通知 HA 实体更新状态
   Notify HA entities to update state

工作原理 | How it works:
- 继承自 DataUpdateCoordinator，这是 HA 推荐的数据管理方式
  Inherits from DataUpdateCoordinator, the recommended data management approach in HA
- 设备主动推送数据（push 模式），而不是轮询（poll 模式）
  Device actively pushes data (push mode), instead of polling (poll mode)
- 当收到新数据时，自动通知所有相关实体更新
  When new data is received, automatically notifies all related entities to update
"""
from __future__ import annotations

import asyncio
import logging
from typing import Any

from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant, callback
from homeassistant.helpers.update_coordinator import DataUpdateCoordinator

from .const import DOMAIN
from .device import SeeedHADevice

# 创建日志记录器
_LOGGER = logging.getLogger(__name__)


class SeeedHACoordinator(DataUpdateCoordinator[dict[str, Any]]):
    """
    Seeed HA 数据协调器
    Class to manage fetching Seeed HA data.

    这个类负责：
    1. 管理设备连接的生命周期
    2. 处理设备推送的状态更新
    3. 通知 HA 实体刷新状态

    与标准的轮询模式不同，这里使用推送模式：
    - 设备通过 WebSocket 主动推送数据
    - 协调器收到数据后调用 async_set_updated_data()
    - HA 自动通知所有监听的实体
    """

    def __init__(
        self,
        hass: HomeAssistant,
        device: SeeedHADevice,
        entry: ConfigEntry,
    ) -> None:
        """
        初始化协调器
        Initialize the coordinator.

        参数 | Args:
            hass: Home Assistant 实例
            device: Seeed HA 设备实例
            entry: 配置入口
        """
        # 调用父类初始化
        # 注意：不设置 update_interval，因为我们使用推送模式
        super().__init__(
            hass,
            _LOGGER,
            name=DOMAIN,
        )

        self.device = device
        self.entry = entry

        # 回调清理函数
        self._remove_state_callback: callable | None = None
        self._remove_discovery_callback: callable | None = None

        # 发现完成事件 - 用于等待设备报告其实体
        self._discovery_complete = asyncio.Event()

    async def async_connect(self) -> None:
        """
        连接到设备并设置回调
        Connect to the device and set up callbacks.

        这个方法在集成初始化时被调用。
        执行以下步骤：
        1. 注册状态和发现回调
        2. 连接到设备
        3. 等待发现完成
        4. 更新初始数据
        """
        # 协调器开始连接 | Coordinator starting connection
        _LOGGER.info("Coordinator starting connection")

        # 步骤 1: 注册回调
        # 当设备推送状态更新时，会调用 _handle_state_update
        self._remove_state_callback = self.device.add_state_callback(
            self._handle_state_update
        )
        # 当设备报告其实体时，会调用 _handle_discovery
        self._remove_discovery_callback = self.device.add_discovery_callback(
            self._handle_discovery
        )

        # 步骤 2: 连接到设备 | Step 2: Connect to device
        if not await self.device.async_connect():
            raise ConnectionError("Cannot connect to Seeed HA device")

        # 步骤 3: 等待发现完成（最多等待 10 秒）| Step 3: Wait for discovery (max 10 seconds)
        try:
            await asyncio.wait_for(self._discovery_complete.wait(), timeout=10)
            _LOGGER.info("Device discovery complete, %d entities found", len(self.device.entities))
        except asyncio.TimeoutError:
            _LOGGER.warning("Device discovery timeout, continuing with existing entities")

        # 步骤 4: 设置初始数据
        self.async_set_updated_data({"entities": self.device.entities})

    async def async_disconnect(self) -> None:
        """
        断开连接并清理资源
        Disconnect from the device and clean up.

        移除所有回调并断开设备连接。
        """
        # 协调器断开连接 | Coordinator disconnecting
        _LOGGER.info("Coordinator disconnecting")

        # 移除状态回调
        if self._remove_state_callback:
            self._remove_state_callback()
            self._remove_state_callback = None

        # 移除发现回调
        if self._remove_discovery_callback:
            self._remove_discovery_callback()
            self._remove_discovery_callback = None

        # 断开设备连接
        await self.device.async_disconnect()

    @callback
    def _handle_state_update(self, data: dict[str, Any]) -> None:
        """
        处理设备的状态更新
        Handle state update from device.

        当设备推送新的传感器数据时，这个回调会被触发。
        更新协调器数据，这会自动通知所有监听的实体。

        使用 @callback 装饰器表示这是一个同步回调，
        在主事件循环中执行，不需要 await。

        参数 | Args:
            data: 状态更新数据
                  格式: {type: "state", entity_id: "xxx", state: xxx, attributes: {...}}
        """
        entity_id = data.get("entity_id")
        state = data.get("state")

        # 收到状态更新 | Received state update
        _LOGGER.debug("Received state update: %s = %s", entity_id, state)

        # 更新协调器数据，触发实体刷新
        self.async_set_updated_data({"entities": self.device.entities})

    @callback
    def _handle_discovery(self, data: dict[str, Any]) -> None:
        """
        处理设备的发现信息
        Handle discovery from device.

        当设备报告其支持的实体列表时，这个回调会被触发。
        设置发现完成事件，并更新数据。

        参数 | Args:
            data: 发现数据
                  格式: {type: "discovery", entities: [{id, name, type, ...}, ...]}
        """
        entities = data.get("entities", [])
        # 收到设备发现 | Received device discovery
        _LOGGER.info("Received device discovery: %d entities", len(entities))

        for entity in entities:
            _LOGGER.debug("Entity: %s", entity)

        # 标记发现完成
        self._discovery_complete.set()

        # 更新数据
        self.async_set_updated_data({"entities": self.device.entities})

    async def _async_update_data(self) -> dict[str, Any]:
        """
        获取最新数据（轮询模式）
        Fetch data from the device.

        注意：这个方法在推送模式下不会被调用。
        我们使用 async_set_updated_data() 主动更新数据。

        返回 | Returns:
            dict: 包含实体数据的字典
        """
        return {"entities": self.device.entities}
