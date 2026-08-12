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
from homeassistant.const import ATTR_ENTITY_ID
from homeassistant.core import HomeAssistant
from homeassistant.helpers.aiohttp_client import async_get_clientsession

from homeassistant.helpers.event import async_track_state_change_event

from .const import (
    MSG_TYPE_PING,
    MSG_TYPE_PONG,
    MSG_TYPE_STATE,
    MSG_TYPE_DISCOVERY,
    MSG_TYPE_COMMAND,
    MSG_TYPE_HA_STATE,
    MSG_TYPE_HA_STATE_CLEAR,
    MSG_TYPE_HA_ENTITY_COMMAND,
    MSG_TYPE_HA_ENTITY_COMMAND_RESULT,
    MSG_TYPE_SLEEP,
    MSG_TYPE_IR_TRANSMIT,
    MSG_TYPE_IR_TRANSMIT_RESULT,
    MSG_TYPE_IR_LEARN_START,
    MSG_TYPE_IR_LEARN_CANCEL,
    MSG_TYPE_IR_LEARN_RESULT,
    MSG_TYPE_IR_BUILTIN_COMMAND,
    MSG_TYPE_IR_TOUCH_BINDING_SET,
    MSG_TYPE_IR_TOUCH_BINDING_RESULT,
    MSG_TYPE_IR_TOUCH_STATUS_REQUEST,
    MSG_TYPE_IR_TOUCH_STATUS_RESULT,
    MSG_TYPE_IR_GESTURE,
    HEARTBEAT_INTERVAL,
    RECONNECT_INTERVAL,
    DEFAULT_HTTP_PORT,
)
from .entity_control import EntityCommandError, parse_entity_command

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
        # Infrared signal callbacks | 红外信号回调
        self._infrared_receive_callbacks: list[
            Callable[[dict[str, Any]], None]
        ] = []
        # Physical touch gesture callbacks (coroutine functions) | 物理触摸手势回调(协程函数)
        self._infrared_gesture_callbacks: list[
            Callable[[str], Any]
        ] = []

        # 设备上报的实体数据
        # 格式: {entity_id: {type, name, state, unit, ...}}
        self._entities: dict[str, dict[str, Any]] = {}

        # 设备基本信息（型号、版本等）
        self._device_info: dict[str, Any] = {}

        # Pending infrared transmissions keyed by request ID
        # 以请求 ID 为键保存正在等待的红外发射任务
        self._pending_ir_transmits: dict[
            int, asyncio.Future[dict[str, Any]]
        ] = {}
        # Pending infrared learning sessions keyed by request ID
        # 以请求 ID 为键保存正在等待的红外学习任务
        self._pending_ir_learns: dict[int, asyncio.Future[dict[str, Any]]] = {}
        # Pending offline touch profile requests keyed by request ID
        # 以请求 ID 为键保存正在等待的离线触摸配置任务
        self._pending_ir_touch_requests: dict[
            int, asyncio.Future[dict[str, Any]]
        ] = {}
        self._next_request_id = 1

        # =========================================================================
        # HA 实体订阅相关 | HA Entity Subscription
        # =========================================================================
        
        # 订阅的 HA 实体列表 | Subscribed HA entities
        self._subscribed_entities: list[str] = []
        # 状态监听取消函数 | State listener cancel function
        self._state_unsub: Callable[[], None] | None = None

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

    def add_infrared_receive_callback(
        self, callback: Callable[[dict[str, Any]], None]
    ) -> Callable[[], None]:
        """Register a callback for raw infrared receive events."""
        self._infrared_receive_callbacks.append(callback)

        def remove_callback() -> None:
            """Remove the infrared receive callback."""
            self._infrared_receive_callbacks.remove(callback)

        return remove_callback

    def add_infrared_gesture_callback(
        self, callback: Callable[[str], Any]
    ) -> Callable[[], None]:
        """Register a coroutine callback for physical touch gesture events."""
        self._infrared_gesture_callbacks.append(callback)

        def remove_callback() -> None:
            """Remove the infrared gesture callback."""
            if callback in self._infrared_gesture_callbacks:
                self._infrared_gesture_callbacks.remove(callback)

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

            # 步骤 5: 如果有订阅实体，推送当前状态（确保设备重启后能收到状态）
            # Step 5: If there are subscribed entities, push current states
            # (ensures device receives states after restart)
            if self._subscribed_entities:
                _LOGGER.info("Pushing %d subscribed entity states after connect", 
                           len(self._subscribed_entities))
                # 延迟一小段时间让设备准备好接收
                await asyncio.sleep(0.5)
                for entity_id in self._subscribed_entities:
                    state = self.hass.states.get(entity_id)
                    if state:
                        await self._async_push_ha_state(entity_id, state)
                    else:
                        _LOGGER.warning("Entity not found: %s", entity_id)

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
        self._fail_pending_ir_transmits(ConnectionError("Device disconnected"))
        self._fail_pending_ir_learns(ConnectionError("Device disconnected"))
        self._fail_pending_ir_touch_requests(
            ConnectionError("Device disconnected")
        )

        # 取消 HA 实体状态监听 | Cancel HA entity state listener
        if self._state_unsub:
            self._state_unsub()
            self._state_unsub = None

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
            self._fail_pending_ir_transmits(ConnectionError("Device disconnected"))
            self._fail_pending_ir_learns(ConnectionError("Device disconnected"))
            self._fail_pending_ir_touch_requests(
                ConnectionError("Device disconnected")
            )
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

        elif msg_type == MSG_TYPE_SLEEP:
            # 设备休眠通知 - 立即标记断开并开始重连
            # Device sleep notification - immediately mark disconnected and start reconnect
            _LOGGER.info("Device entering sleep mode: %s", self.host)
            self._connected = False
            
            # 关闭当前 WebSocket 连接
            if self._ws and not self._ws.closed:
                await self._ws.close()
            
            # 立即启动重连任务
            if not self._reconnect_task:
                self._reconnect_task = asyncio.create_task(self._async_reconnect())

        elif msg_type == MSG_TYPE_HA_ENTITY_COMMAND:
            await self._async_handle_ha_entity_command(data)

        elif msg_type == MSG_TYPE_IR_TRANSMIT_RESULT:
            request_id = data.get("request_id")
            future = self._pending_ir_transmits.get(request_id)
            if future is not None and not future.done():
                future.set_result(data)

        elif msg_type == MSG_TYPE_IR_LEARN_RESULT:
            request_id = data.get("request_id")
            if data.get("success", False):
                timings = data.get("timings")
                if not isinstance(timings, list) or not timings or not all(
                    isinstance(value, int) for value in timings
                ):
                    _LOGGER.warning("Received invalid infrared signal payload")
                    data = {
                        "type": MSG_TYPE_IR_LEARN_RESULT,
                        "request_id": request_id,
                        "success": False,
                        "error": "invalid_signal_payload",
                    }

            future = self._pending_ir_learns.get(request_id)
            if future is not None and not future.done():
                future.set_result(data)

            if not data.get("success", False):
                return

            for callback in tuple(self._infrared_receive_callbacks):
                try:
                    callback(data)
                except Exception as err:
                    _LOGGER.error("Infrared receive callback error: %s", err)

        elif msg_type in (
            MSG_TYPE_IR_TOUCH_BINDING_RESULT,
            MSG_TYPE_IR_TOUCH_STATUS_RESULT,
        ):
            request_id = data.get("request_id")
            future = self._pending_ir_touch_requests.get(request_id)
            if future is not None and not future.done():
                future.set_result(data)

        elif msg_type == MSG_TYPE_IR_GESTURE:
            # A physical gesture fired on the device; hand it to the IR manager
            # as a background task so a long transmission never blocks receiving.
            # 设备触发了物理手势;作为后台任务交给 IR 管理器,避免长发射阻塞接收。
            gesture = data.get("gesture")
            if isinstance(gesture, str):
                for callback in tuple(self._infrared_gesture_callbacks):
                    self.hass.async_create_task(callback(gesture))

    async def _async_handle_ha_entity_command(
        self, data: dict[str, Any]
    ) -> None:
        """Validate and execute an entity action requested by the device."""

        request_id = data.get("request_id")
        try:
            command = parse_entity_command(data, self._subscribed_entities)
            await self.hass.services.async_call(
                "homeassistant",
                command.action,
                {ATTR_ENTITY_ID: list(command.entity_ids)},
                blocking=True,
            )
        except EntityCommandError as err:
            _LOGGER.warning(
                "Rejected HA entity command from %s: %s", self.host, err.code
            )
            await self._async_send_entity_command_result(
                request_id, False, err.code
            )
            return
        except Exception as err:
            _LOGGER.error(
                "HA entity command failed for %s: %s", self.host, err
            )
            await self._async_send_entity_command_result(
                request_id, False, "service_call_failed"
            )
            return

        _LOGGER.info(
            "Executed HA entity command from %s: %s -> %s",
            self.host,
            command.action,
            command.entity_ids,
        )
        await self._async_send_entity_command_result(
            command.request_id, True, ""
        )

    async def _async_send_entity_command_result(
        self, request_id: Any, success: bool, error: str
    ) -> None:
        """Return one stable control result to the requesting device."""

        await self._async_send(
            {
                "type": MSG_TYPE_HA_ENTITY_COMMAND_RESULT,
                "request_id": request_id if type(request_id) is int else 0,
                "success": success,
                "error": error,
            }
        )

    async def _async_reconnect(self) -> None:
        """
        自动重连（固定间隔）
        Reconnect to the device (fixed interval).
        """
        while not self._connected:
            if await self.async_connect():
                # 状态推送已在 async_connect 中完成，无需再次调用
                # State push is already done in async_connect, no need to call again
                break
            
            await asyncio.sleep(RECONNECT_INTERVAL)

        self._reconnect_task = None

    async def _async_restore_entity_subscription(self) -> None:
        """
        恢复实体订阅（重连后调用）
        Restore entity subscription (called after reconnect).

        不需要重新设置状态监听器，因为它们仍然有效。
        只需要重新推送当前状态到设备。
        No need to re-setup state listeners as they are still valid.
        Just need to re-push current states to device.
        """
        # 发送清除消息 | Send clear message
        await self._async_send_ha_state_clear()
        
        # 重新推送所有订阅实体的当前状态 | Re-push current states of all subscribed entities
        for entity_id in self._subscribed_entities:
            state = self.hass.states.get(entity_id)
            if state:
                await self._async_push_ha_state(entity_id, state)
            else:
                _LOGGER.warning("Entity not found during restore: %s", entity_id)

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

    async def async_transmit_infrared(
        self,
        carrier_frequency: int,
        timings: list[int],
        repeat_count: int = 0,
    ) -> None:
        """Transmit raw infrared timings and wait for the device result."""
        if not timings:
            raise ValueError("Infrared timings cannot be empty")
        if not 1000 <= carrier_frequency <= 65535:
            raise ValueError("Infrared carrier frequency is out of range")
        if not 0 <= repeat_count <= 10:
            raise ValueError("Infrared repeat count is out of range")
        if not all(isinstance(value, int) for value in timings):
            raise ValueError("Infrared timings must contain integers")

        request_id = self._next_request_id
        self._next_request_id = 1 if request_id >= 0x7FFFFFFF else request_id + 1

        future = asyncio.get_running_loop().create_future()
        self._pending_ir_transmits[request_id] = future

        sent = await self._async_send(
            {
                "type": MSG_TYPE_IR_TRANSMIT,
                "request_id": request_id,
                "carrier_frequency": carrier_frequency,
                "repeat_count": repeat_count,
                "timings": timings,
            }
        )
        if not sent:
            self._pending_ir_transmits.pop(request_id, None)
            raise ConnectionError("Infrared transmit request could not be sent")

        try:
            result = await asyncio.wait_for(future, timeout=15)
        finally:
            self._pending_ir_transmits.pop(request_id, None)

        if not result.get("success", False):
            error = result.get("error", "unknown_error")
            raise RuntimeError(f"Infrared transmission failed: {error}")

    async def async_execute_ir_builtin(self, action: str) -> None:
        """Execute one firmware-backed infrared action."""
        if not action:
            raise ValueError("Built-in infrared action cannot be empty")

        request_id = self._next_request_id
        self._next_request_id = 1 if request_id >= 0x7FFFFFFF else request_id + 1
        future = asyncio.get_running_loop().create_future()
        self._pending_ir_transmits[request_id] = future

        sent = await self._async_send(
            {
                "type": MSG_TYPE_IR_BUILTIN_COMMAND,
                "request_id": request_id,
                "action": action,
            }
        )
        if not sent:
            self._pending_ir_transmits.pop(request_id, None)
            raise ConnectionError("Built-in infrared request could not be sent")

        try:
            result = await asyncio.wait_for(future, timeout=15)
        finally:
            self._pending_ir_transmits.pop(request_id, None)
        if not result.get("success", False):
            error = result.get("error", "unknown_error")
            raise RuntimeError(f"Built-in infrared action failed: {error}")

    async def async_learn_infrared(self, timeout: int) -> dict[str, Any]:
        """Start one device-side learning session and return its raw signal."""
        if not 1 <= timeout <= 60:
            raise ValueError(
                "Infrared learning timeout must be between 1 and 60 seconds"
            )

        request_id = self._next_request_id
        self._next_request_id = 1 if request_id >= 0x7FFFFFFF else request_id + 1

        future = asyncio.get_running_loop().create_future()
        self._pending_ir_learns[request_id] = future

        sent = await self._async_send(
            {
                "type": MSG_TYPE_IR_LEARN_START,
                "request_id": request_id,
                "timeout_ms": timeout * 1000,
            }
        )
        if not sent:
            self._pending_ir_learns.pop(request_id, None)
            raise ConnectionError("Infrared learning request could not be sent")

        try:
            result = await asyncio.wait_for(future, timeout=timeout + 2)
        except (TimeoutError, asyncio.CancelledError):
            await self._async_send(
                {
                    "type": MSG_TYPE_IR_LEARN_CANCEL,
                    "request_id": request_id,
                }
            )
            raise
        finally:
            self._pending_ir_learns.pop(request_id, None)

        if not result.get("success", False):
            error = result.get("error", "unknown_error")
            if error == "learning_timeout":
                raise TimeoutError
            raise RuntimeError(f"Infrared learning failed: {error}")
        return result

    async def async_set_ir_touch_binding(
        self,
        gesture: str,
        binding: dict[str, Any],
        revision: int,
        final: bool,
    ) -> dict[str, Any]:
        """Write one gesture binding and wait for the device acknowledgement."""
        if gesture not in {"single", "double", "triple", "quadruple"}:
            raise ValueError("Unsupported touch gesture")
        if binding.get("source") not in {"none", "builtin", "raw", "managed"}:
            raise ValueError("Unsupported touch binding source")

        request_id = self._next_request_id
        self._next_request_id = 1 if request_id >= 0x7FFFFFFF else request_id + 1
        future = asyncio.get_running_loop().create_future()
        self._pending_ir_touch_requests[request_id] = future
        sent = await self._async_send(
            {
                "type": MSG_TYPE_IR_TOUCH_BINDING_SET,
                "request_id": request_id,
                "revision": revision,
                "gesture": gesture,
                "binding": binding,
                "final": final,
            }
        )
        if not sent:
            self._pending_ir_touch_requests.pop(request_id, None)
            raise ConnectionError("Touch binding request could not be sent")
        try:
            result = await asyncio.wait_for(future, timeout=15)
        finally:
            self._pending_ir_touch_requests.pop(request_id, None)
        if not result.get("success", False):
            error = result.get("error", "unknown_error")
            raise RuntimeError(f"Touch binding synchronization failed: {error}")
        return result

    async def async_get_ir_touch_status(self) -> dict[str, Any]:
        """Read the offline touch profile revision from the device."""
        request_id = self._next_request_id
        self._next_request_id = 1 if request_id >= 0x7FFFFFFF else request_id + 1
        future = asyncio.get_running_loop().create_future()
        self._pending_ir_touch_requests[request_id] = future
        sent = await self._async_send(
            {
                "type": MSG_TYPE_IR_TOUCH_STATUS_REQUEST,
                "request_id": request_id,
            }
        )
        if not sent:
            self._pending_ir_touch_requests.pop(request_id, None)
            raise ConnectionError("Touch status request could not be sent")
        try:
            result = await asyncio.wait_for(future, timeout=10)
        finally:
            self._pending_ir_touch_requests.pop(request_id, None)
        if not result.get("success", False):
            error = result.get("error", "unknown_error")
            raise RuntimeError(f"Touch status request failed: {error}")
        return result

    def _fail_pending_ir_transmits(self, error: Exception) -> None:
        """Fail all infrared transmissions waiting for a device response."""
        for future in self._pending_ir_transmits.values():
            if not future.done():
                future.set_exception(error)
        self._pending_ir_transmits.clear()

    def _fail_pending_ir_learns(self, error: Exception) -> None:
        """Fail all infrared learning sessions waiting for a device response."""
        for future in self._pending_ir_learns.values():
            if not future.done():
                future.set_exception(error)
        self._pending_ir_learns.clear()

    def _fail_pending_ir_touch_requests(self, error: Exception) -> None:
        """Fail all touch profile requests waiting for a device response."""
        for future in self._pending_ir_touch_requests.values():
            if not future.done():
                future.set_exception(error)
        self._pending_ir_touch_requests.clear()

    # =========================================================================
    # HA 实体状态订阅 | HA Entity State Subscription
    # =========================================================================

    async def async_setup_entity_subscription(
        self,
        entity_ids: list[str]
    ) -> None:
        """
        设置 HA 实体状态订阅
        Set up HA entity state subscription.

        监听指定 HA 实体的状态变化，并推送给 Arduino 设备。
        Listen to state changes of specified HA entities and push to Arduino device.

        参数 | Args:
            entity_ids: 要订阅的 HA 实体 ID 列表
                        List of HA entity IDs to subscribe

        示例 | Example:
            await device.async_setup_entity_subscription([
                "sensor.living_room_temperature",
                "sensor.outdoor_humidity"
            ])
        """
        # 取消之前的订阅 | Cancel previous subscription
        if self._state_unsub:
            self._state_unsub()
            self._state_unsub = None

        # 发送清除消息到 Arduino，清除旧的订阅状态
        # Send clear message to Arduino to clear old subscribed states
        await self._async_send_ha_state_clear()

        self._subscribed_entities = entity_ids

        if not entity_ids:
            _LOGGER.info("No entities to subscribe, cleared all")
            return

        _LOGGER.info("Setting up HA entity subscription: %s", entity_ids)

        # 监听实体状态变化 | Listen to entity state changes
        self._state_unsub = async_track_state_change_event(
            self.hass,
            entity_ids,
            self._async_handle_ha_state_change,
        )

        # 发送所有实体的初始状态 | Send initial state of all entities
        for entity_id in entity_ids:
            state = self.hass.states.get(entity_id)
            if state:
                await self._async_push_ha_state(entity_id, state)
            else:
                _LOGGER.warning("Entity not found: %s", entity_id)

    async def _async_handle_ha_state_change(self, event) -> None:
        """
        处理 HA 实体状态变化事件
        Handle HA entity state change event.

        当订阅的 HA 实体状态变化时，此方法会被调用。
        This method is called when a subscribed HA entity state changes.
        """
        entity_id = event.data.get("entity_id")
        new_state = event.data.get("new_state")

        if new_state is None:
            # 实体被删除 | Entity was removed
            _LOGGER.debug("Entity removed: %s", entity_id)
            return

        await self._async_push_ha_state(entity_id, new_state)

    async def _async_push_ha_state(
        self,
        entity_id: str,
        state
    ) -> None:
        """
        推送 HA 实体状态到 Arduino 设备
        Push HA entity state to Arduino device.

        参数 | Args:
            entity_id: HA 实体 ID
            state: HA 状态对象
        """
        # 构建推送数据 | Build push data
        data = {
            "type": MSG_TYPE_HA_STATE,
            "entity_id": entity_id,
            "state": state.state,
            "attributes": {
                "friendly_name": state.attributes.get("friendly_name", entity_id),
                "unit_of_measurement": state.attributes.get("unit_of_measurement", ""),
                "device_class": state.attributes.get("device_class", ""),
                "icon": state.attributes.get("icon", ""),
            }
        }

        _LOGGER.debug("Pushing HA state to device: %s = %s", entity_id, state.state)
        await self._async_send(data)

    async def _async_send_ha_state_clear(self) -> None:
        """
        发送清除 HA 状态消息到 Arduino 设备
        Send clear HA states message to Arduino device.

        通知 Arduino 清除所有已存储的 HA 实体状态。
        Notify Arduino to clear all stored HA entity states.
        """
        data = {
            "type": MSG_TYPE_HA_STATE_CLEAR,
        }

        _LOGGER.debug("Sending HA state clear to device")
        await self._async_send(data)
