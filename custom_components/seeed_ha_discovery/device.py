"""
Seeed HA Discovery - 设备通信模块
Seeed HA Discovery - Device communication module.

这个模块负责与 ESP32 设备的所有通信：
This module handles all communication with ESP32 devices:
1. 建立 WebSocket 连接
   Establish WebSocket connection
2. 接收传感器数据更新
   Receive sensor data updates
3. 处理心跳保活
   Handle heartbeat keep-alive
4. 自动重连机制
   Auto-reconnect mechanism

通信协议 | Communication protocol:
- 使用 WebSocket 进行实时双向通信
  Use WebSocket for real-time bidirectional communication
- 数据格式为 JSON
  Data format is JSON
- 支持 ping/pong 心跳检测
  Supports ping/pong heartbeat detection
- 设备主动推送传感器状态更新
  Device actively pushes sensor state updates
"""
from __future__ import annotations

import asyncio
import json
import logging
from typing import Any, Callable

import aiohttp

from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant
from homeassistant.helpers.aiohttp_client import async_get_clientsession

from .const import (
    MSG_TYPE_PING,
    MSG_TYPE_PONG,
    MSG_TYPE_STATE,
    MSG_TYPE_DISCOVERY,
    MSG_TYPE_COMMAND,
    HEARTBEAT_INTERVAL,
    RECONNECT_INTERVAL,
    DEFAULT_HTTP_PORT,
)

# 创建日志记录器
_LOGGER = logging.getLogger(__name__)


class SeeedHADevice:
    """
    Seeed HA 设备类
    Represents a Seeed HA device.

    这个类封装了与单个 ESP32 设备的所有通信逻辑。
    每个连接的设备都有一个对应的实例。
    """

    def __init__(
        self,
        hass: HomeAssistant,
        host: str,
        port: int,
        entry: ConfigEntry,
    ) -> None:
        """
        初始化设备实例
        Initialize the device.

        参数 | Args:
            hass: Home Assistant 实例
            host: 设备 IP 地址
            port: WebSocket 端口
            entry: 配置入口
        """
        self.hass = hass
        self.host = host
        self.port = port
        self.entry = entry

        # WebSocket 连接对象
        self._ws: aiohttp.ClientWebSocketResponse | None = None

        # 连接状态
        self._connected = False

        # 后台任务
        self._reconnect_task: asyncio.Task | None = None  # 重连任务
        self._receive_task: asyncio.Task | None = None     # 消息接收任务

        # 回调函数列表
        # 状态更新回调 - 当收到传感器数据时调用
        self._state_callbacks: list[Callable[[dict[str, Any]], None]] = []
        # 发现回调 - 当收到设备实体列表时调用
        self._discovery_callbacks: list[Callable[[dict[str, Any]], None]] = []

        # 设备上报的实体数据
        # 格式: {entity_id: {type, name, state, unit, ...}}
        self._entities: dict[str, dict[str, Any]] = {}

        # 设备基本信息（型号、版本等）
        self._device_info: dict[str, Any] = {}

    @property
    def connected(self) -> bool:
        """
        获取连接状态
        Return if device is connected.
        """
        return self._connected

    @property
    def device_info(self) -> dict[str, Any]:
        """
        获取设备信息
        Return device info.
        """
        return self._device_info

    @property
    def entities(self) -> dict[str, dict[str, Any]]:
        """
        获取已发现的实体
        Return discovered entities.
        """
        return self._entities

    def add_state_callback(
        self, callback: Callable[[dict[str, Any]], None]
    ) -> Callable[[], None]:
        """
        添加状态更新回调
        Add a state update callback.

        当收到传感器数据更新时，会调用注册的回调函数。
        Registered callbacks will be called when sensor data is received.

        参数 | Args:
            callback: 回调函数，接收状态数据字典

        返回 | Returns:
            移除回调的函数 | Function to remove the callback
        """
        self._state_callbacks.append(callback)

        def remove_callback() -> None:
            """移除回调 | Remove callback"""
            self._state_callbacks.remove(callback)

        return remove_callback

    def add_discovery_callback(
        self, callback: Callable[[dict[str, Any]], None]
    ) -> Callable[[], None]:
        """
        添加发现回调
        Add a discovery callback.

        当收到设备实体发现信息时，会调用注册的回调函数。
        Registered callbacks will be called when entity discovery is received.

        参数 | Args:
            callback: 回调函数，接收发现数据字典

        返回 | Returns:
            移除回调的函数 | Function to remove the callback
        """
        self._discovery_callbacks.append(callback)

        def remove_callback() -> None:
            """移除回调 | Remove callback"""
            self._discovery_callbacks.remove(callback)

        return remove_callback

    async def async_connect(self) -> bool:
        """
        连接到设备
        Connect to the device.

        执行以下步骤：
        1. 通过 HTTP 获取设备信息
        2. 建立 WebSocket 连接
        3. 启动消息接收任务
        4. 请求设备发送实体发现信息

        返回 | Returns:
            bool: 连接是否成功
        """
        try:
            # 步骤 1: 获取设备信息 | Step 1: Get device info
            _LOGGER.info("Getting device info: %s", self.host)
            await self._async_fetch_device_info()

            # 步骤 2: 建立 WebSocket 连接 | Step 2: Establish WebSocket connection
            session = async_get_clientsession(self.hass)
            ws_url = f"ws://{self.host}:{self.port}/ws"

            _LOGGER.info("Connecting to WebSocket: %s", ws_url)

            self._ws = await session.ws_connect(
                ws_url,
                heartbeat=HEARTBEAT_INTERVAL,  # 自动心跳
                timeout=aiohttp.ClientTimeout(total=10),
            )

            self._connected = True
            # WebSocket 连接成功 | WebSocket connected
            _LOGGER.info("WebSocket connected: %s", self.host)

            # 步骤 3: 启动消息接收循环
            self._receive_task = asyncio.create_task(self._async_receive_loop())

            # 步骤 4: 请求设备发送实体信息
            await self.async_request_discovery()

            return True

        except Exception as err:
            # 连接失败 | Connection failed
            _LOGGER.error("Connection failed %s: %s", self.host, err)
            self._connected = False
            return False

    async def async_disconnect(self) -> None:
        """
        断开与设备的连接
        Disconnect from the device.

        清理所有后台任务和连接资源。
        Cleans up all background tasks and connection resources.
        """
        # 正在断开连接 | Disconnecting
        _LOGGER.info("Disconnecting: %s", self.host)
        self._connected = False

        # 取消接收任务
        if self._receive_task:
            self._receive_task.cancel()
            try:
                await self._receive_task
            except asyncio.CancelledError:
                pass
            self._receive_task = None

        # 取消重连任务
        if self._reconnect_task:
            self._reconnect_task.cancel()
            try:
                await self._reconnect_task
            except asyncio.CancelledError:
                pass
            self._reconnect_task = None

        # 关闭 WebSocket 连接
        if self._ws and not self._ws.closed:
            await self._ws.close()
            self._ws = None

        # 已断开连接 | Disconnected
        _LOGGER.info("Disconnected: %s", self.host)

    async def _async_fetch_device_info(self) -> None:
        """
        通过 HTTP 获取设备信息
        Fetch device info via HTTP.

        访问设备的 /info 端点获取基本信息。
        """
        session = async_get_clientsession(self.hass)
        url = f"http://{self.host}:{DEFAULT_HTTP_PORT}/info"

        try:
            async with asyncio.timeout(10):
                async with session.get(url) as response:
                    if response.status == 200:
                        self._device_info = await response.json()
                        _LOGGER.info("Device info: %s", self._device_info)
                    else:
                        # 获取设备信息失败 | Failed to get device info
                        _LOGGER.warning("Failed to get device info, status: %s", response.status)
        except Exception as err:
            # 获取设备信息时出错 | Error getting device info
            _LOGGER.warning("Error getting device info: %s", err)

    async def _async_receive_loop(self) -> None:
        """
        WebSocket 消息接收循环
        Receive messages from WebSocket.

        持续监听 WebSocket 消息，处理不同类型的消息：
        - TEXT: JSON 数据消息
        - ERROR: 连接错误
        - CLOSED: 连接关闭

        断开后会自动触发重连。
        """
        if not self._ws:
            return

        # 开始消息接收循环 | Starting message receive loop
        _LOGGER.debug("Starting message receive loop")

        try:
            async for msg in self._ws:
                if msg.type == aiohttp.WSMsgType.TEXT:
                    # 收到文本消息，解析 JSON | Received text message, parse JSON
                    try:
                        data = json.loads(msg.data)
                        await self._async_handle_message(data)
                    except json.JSONDecodeError:
                        _LOGGER.warning("Received invalid JSON: %s", msg.data)

                elif msg.type == aiohttp.WSMsgType.ERROR:
                    # WebSocket 错误 | WebSocket error
                    _LOGGER.error("WebSocket error: %s", self._ws.exception())
                    break

                elif msg.type == aiohttp.WSMsgType.CLOSED:
                    # 连接被关闭 | Connection closed
                    _LOGGER.info("WebSocket connection closed")
                    break

        except asyncio.CancelledError:
            # 任务被取消（正常关闭）| Task cancelled (normal shutdown)
            raise
        except Exception as err:
            _LOGGER.error("Error receiving messages: %s", err)

        finally:
            # 连接断开，触发重连
            self._connected = False
            if not self._reconnect_task:
                self._reconnect_task = asyncio.create_task(self._async_reconnect())

    async def _async_handle_message(self, data: dict[str, Any]) -> None:
        """
        处理接收到的消息
        Handle incoming message.

        根据消息类型分发处理：
        - ping: 响应心跳
        - state: 更新实体状态
        - discovery: 处理实体发现

        参数 | Args:
            data: 解析后的 JSON 数据
        """
        msg_type = data.get("type")
        # 收到消息 | Received message
        _LOGGER.debug("Received message: type=%s, data=%s", msg_type, data)

        if msg_type == MSG_TYPE_PING:
            # 心跳请求，发送响应
            await self._async_send({
                "type": MSG_TYPE_PONG,
                "timestamp": data.get("timestamp"),
            })

        elif msg_type == MSG_TYPE_STATE:
            # 状态更新消息
            # 格式: {type: "state", entity_id: "xxx", state: xxx, attributes: {...}}
            entity_id = data.get("entity_id")
            state = data.get("state")
            attributes = data.get("attributes", {})

            if entity_id:
                # 更新本地实体数据
                if entity_id not in self._entities:
                    self._entities[entity_id] = {}

                self._entities[entity_id]["state"] = state
                self._entities[entity_id]["attributes"] = attributes

                # 实体状态更新 | Entity state updated
                _LOGGER.info("Entity state updated: %s = %s", entity_id, state)

            # 通知所有状态回调 | Notify all state callbacks
            for callback in self._state_callbacks:
                try:
                    callback(data)
                except Exception as err:
                    _LOGGER.error("State callback error: %s", err)

        elif msg_type == MSG_TYPE_DISCOVERY:
            # 实体发现消息
            # 格式: {type: "discovery", entities: [{id, name, type, unit, ...}, ...]}
            # Format: {type: "discovery", entities: [{id, name, type, unit, ...}, ...]}
            entities = data.get("entities", [])

            # 收到实体发现 | Received entity discovery
            _LOGGER.info("Received entity discovery: %d entities", len(entities))

            for entity in entities:
                entity_id = entity.get("id")
                if entity_id:
                    self._entities[entity_id] = entity
                    _LOGGER.debug("Discovered entity: %s", entity)

            # 通知所有发现回调 | Notify all discovery callbacks
            for callback in self._discovery_callbacks:
                try:
                    callback(data)
                except Exception as err:
                    _LOGGER.error("Discovery callback error: %s", err)

    async def _async_reconnect(self) -> None:
        """
        自动重连
        Reconnect to the device.

        在连接断开后持续尝试重连，直到成功。
        Continuously try to reconnect until successful.
        重连间隔由 RECONNECT_INTERVAL 定义。
        Reconnect interval is defined by RECONNECT_INTERVAL.
        """
        while not self._connected:
            # 尝试重连 | Trying to reconnect
            _LOGGER.info("Trying to reconnect: %s", self.host)
            await asyncio.sleep(RECONNECT_INTERVAL)

            if await self.async_connect():
                # 重连成功 | Reconnected successfully
                _LOGGER.info("Reconnected successfully: %s", self.host)
                break

        self._reconnect_task = None

    async def _async_send(self, data: dict[str, Any]) -> bool:
        """
        发送数据到设备
        Send data to the device.

        参数 | Args:
            data: 要发送的数据字典

        返回 | Returns:
            bool: 发送是否成功
        """
        if not self._ws or self._ws.closed:
            # 无法发送: WebSocket 未连接 | Cannot send: WebSocket not connected
            _LOGGER.warning("Cannot send: WebSocket not connected")
            return False

        try:
            await self._ws.send_json(data)
            _LOGGER.debug("Sent data: %s", data)
            return True
        except Exception as err:
            # 发送数据失败 | Failed to send data
            _LOGGER.error("Failed to send data: %s", err)
            return False

    async def async_request_discovery(self) -> bool:
        """
        请求设备发送实体发现信息
        Request entity discovery from device.

        发送发现请求后，设备会回复其支持的所有实体。

        返回 | Returns:
            bool: 请求是否发送成功
        """
        # 请求实体发现 | Request entity discovery
        _LOGGER.info("Requesting entity discovery")
        return await self._async_send({
            "type": MSG_TYPE_DISCOVERY,
            "action": "request",
        })

    async def async_send_command(
        self,
        entity_id: str,
        command: str | None = None,
        state: bool | None = None,
    ) -> bool:
        """
        发送控制命令到设备
        Send control command to device.

        用于控制开关等可执行器。
        Used to control switches and other actuators.

        参数 | Args:
            entity_id: 目标实体 ID
            command: 命令字符串 ("turn_on", "turn_off", "toggle")
            state: 直接指定状态 (True/False)

        返回 | Returns:
            bool: 命令是否发送成功

        示例 | Examples:
            await device.async_send_command("led", command="turn_on")
            await device.async_send_command("led", state=True)
        """
        data: dict[str, Any] = {
            "type": MSG_TYPE_COMMAND,
            "entity_id": entity_id,
        }

        if command is not None:
            data["command"] = command
            # 发送命令 | Sending command
            _LOGGER.info("Sending command: %s -> %s", entity_id, command)
        elif state is not None:
            data["state"] = state
            # 发送状态 | Sending state
            _LOGGER.info("Sending state: %s -> %s", entity_id, state)
        else:
            # 必须提供 command 或 state | Must provide command or state
            _LOGGER.error("Failed to send command: must provide command or state")
            return False

        return await self._async_send(data)
