"""
Seeed HA Discovery - 摄像头平台
Seeed HA Discovery - Camera platform.

这个模块实现摄像头实体，支持：
This module implements camera entities, supporting:
1. WiFi 设备：通过静态图片刷新显示视频
   WiFi devices: Display video via still image refresh
2. 静态图片捕获
   Still image capture

摄像头工作流程 | Camera workflow:
1. 设备通过 WebSocket 发送 discovery 消息，报告其摄像头实体
   Device sends discovery message via WebSocket, reporting its camera entities
2. 本模块根据 discovery 创建对应的摄像头实体
   This module creates corresponding camera entities based on discovery
3. Home Assistant 定时获取静态图片
   Home Assistant periodically fetches still images
4. 实时显示摄像头画面
   Display camera feed in real-time

摄像头数据格式示例（WiFi）| Camera data format example (WiFi):
{
    "id": "camera",
    "name": "Camera",
    "type": "camera",
    "still_url": "/camera"
}
"""
from __future__ import annotations

import asyncio
import logging
from datetime import datetime, timedelta
from typing import Any

import aiohttp

from homeassistant.components.camera import Camera
from homeassistant.config_entries import ConfigEntry
from homeassistant.core import HomeAssistant
from homeassistant.helpers.aiohttp_client import async_get_clientsession
from homeassistant.helpers.device_registry import DeviceInfo
from homeassistant.helpers.entity_platform import AddEntitiesCallback
from homeassistant.helpers.event import async_track_time_interval

from .const import (
    DOMAIN,
    MANUFACTURER,
    CONF_DEVICE_ID,
    CONF_HOST,
    CONF_MODEL,
    CONF_CONNECTION_TYPE,
    CONNECTION_TYPE_WIFI,
    DEFAULT_CAMERA_PORT,
    CAMERA_AVAILABILITY_SCAN_INTERVAL,
)

# 创建日志记录器 | Create logger
_LOGGER = logging.getLogger(__name__)


async def async_setup_entry(
    hass: HomeAssistant,
    entry: ConfigEntry,
    async_add_entities: AddEntitiesCallback,
) -> None:
    """
    设置摄像头平台
    Set up Seeed HA cameras.

    这个函数在集成加载时被调用，根据连接类型选择不同的设置方式。
    This function is called when the integration loads, selecting different setup methods based on connection type.

    参数 | Args:
        hass: Home Assistant 实例 | Home Assistant instance
        entry: 配置入口 | Config entry
        async_add_entities: 添加实体的回调函数 | Callback to add entities
    """
    # 获取连接类型 | Get connection type
    connection_type = entry.data.get(CONF_CONNECTION_TYPE, CONNECTION_TYPE_WIFI)

    # 目前只支持 WiFi 设备的摄像头
    # Currently only WiFi device cameras are supported
    if connection_type == CONNECTION_TYPE_WIFI:
        await _async_setup_wifi_cameras(hass, entry, async_add_entities)


async def _async_setup_wifi_cameras(
    hass: HomeAssistant,
    entry: ConfigEntry,
    async_add_entities: AddEntitiesCallback,
) -> None:
    """
    设置 WiFi 设备摄像头
    Set up WiFi device cameras.
    """
    from .coordinator import SeeedHACoordinator

    # 获取设备数据 | Get device data
    data = hass.data[DOMAIN][entry.entry_id]
    coordinator: SeeedHACoordinator = data["coordinator"]
    host = entry.data.get(CONF_HOST, "")

    # 设置 WiFi 摄像头平台 | Setting up WiFi camera platform
    _LOGGER.info("Setting up WiFi camera platform, device: %s", entry.data.get(CONF_DEVICE_ID))

    # 创建已发现的摄像头实体 | Create discovered camera entities
    entities = []
    for entity_id, entity_config in coordinator.device.entities.items():
        # 只处理 camera 类型的实体 | Only process camera type entities
        if entity_config.get("type") == "camera":
            _LOGGER.info("Creating camera: %s (%s)", entity_id, entity_config.get("name"))
            entities.append(SeeedHACamera(coordinator, entity_config, entry, host))

    # 如果没有发现摄像头实体，但设备是 ESP32-S3，尝试添加默认摄像头
    # If no camera entity discovered but device is ESP32-S3, try adding default camera
    if not entities:
        device_info = coordinator.device.device_info
        model = device_info.get("model", "").lower()
        
        # 检查是否可能有摄像头 | Check if device might have a camera
        if "s3" in model or "sense" in model:
            _LOGGER.info("ESP32-S3 device detected, checking for camera...")
            
            # 尝试连接摄像头端点 | Try to connect to camera endpoint
            if await _check_camera_available(hass, host):
                _LOGGER.info("Camera endpoint found, creating default camera entity")
                default_camera_config = {
                    "id": "camera",
                    "name": "Camera",
                    "type": "camera",
                }
                entities.append(SeeedHACamera(coordinator, default_camera_config, entry, host))

    # 添加实体到 HA | Add entities to HA
    if entities:
        async_add_entities(entities)
        _LOGGER.info("Added %d cameras", len(entities))

    # 注册发现回调，处理后续发现的摄像头 | Register discovery callback
    def handle_discovery(data: dict[str, Any]) -> None:
        """
        处理新发现的摄像头
        Handle newly discovered cameras.
        """
        new_entities = []

        for entity_id, entity_config in coordinator.device.entities.items():
            if entity_config.get("type") == "camera":
                # 检查实体是否已存在 | Check if entity already exists
                existing_ids = [e._entity_id for e in entities]
                if entity_id not in existing_ids:
                    _LOGGER.info("Discovered new camera: %s", entity_id)
                    new_entity = SeeedHACamera(coordinator, entity_config, entry, host)
                    entities.append(new_entity)
                    new_entities.append(new_entity)

        if new_entities:
            async_add_entities(new_entities)
            _LOGGER.info("Dynamically added %d new cameras", len(new_entities))

    # 注册回调 | Register callback
    coordinator.device.add_discovery_callback(handle_discovery)


async def _check_camera_available(hass: HomeAssistant, host: str) -> bool:
    """
    检查摄像头端点是否可用
    Check if camera endpoint is available.
    """
    session = async_get_clientsession(hass)
    camera_url = f"http://{host}:{DEFAULT_CAMERA_PORT}/camera"
    
    try:
        async with asyncio.timeout(5):
            async with session.get(camera_url) as response:
                if response.status == 200:
                    content_type = response.headers.get("Content-Type", "")
                    return "image" in content_type
    except Exception as err:
        _LOGGER.debug("Camera check failed: %s", err)
    
    return False


class SeeedHACamera(Camera):
    """
    Seeed HA WiFi 摄像头实体
    Representation of a Seeed HA WiFi camera.

    这个类代表一个 WiFi 设备的摄像头实体。使用静态图片刷新模式，更稳定可靠。
    This class represents a WiFi device camera entity. Uses still image refresh mode for better stability.

    主要功能 | Main features:
    - 静态图片捕获（自动刷新）| Still image capture (auto refresh)
    - 显示设备信息 | Display device info
    """

    _attr_has_entity_name = True
    # 不使用 STREAM 功能，改用静态图片刷新模式
    # Don't use STREAM feature, use still image refresh mode instead

    # 可用性由 HTTP 取图通道的可达性决定（独立于 WebSocket 传感器通道）
    # Availability is decided by the HTTP image channel reachability
    # (independent of the WebSocket sensor channel)
    _attr_available = True

    def __init__(
        self,
        coordinator,
        entity_config: dict[str, Any],
        entry: ConfigEntry,
        host: str,
    ) -> None:
        """
        初始化摄像头
        Initialize the camera.

        参数 | Args:
            coordinator: 数据协调器 | Data coordinator
            entity_config: 实体配置（来自设备发现）| Entity config (from device discovery)
            entry: 配置入口 | Config entry
            host: 设备 IP 地址 | Device IP address
        """
        super().__init__()

        self._coordinator = coordinator
        self._entry = entry
        self._entity_config = entity_config
        self._host = host

        # 实体 ID（设备上报的 ID）| Entity ID (reported by device)
        self._entity_id = entity_config.get("id", "camera")

        # =====================================================================
        # 设置实体属性 | Set entity attributes
        # =====================================================================

        # 实体名称 - 显示在 UI 上 | Entity name - displayed in UI
        self._attr_name = entity_config.get("name", "Camera")

        # 唯一 ID - 用于 HA 内部识别 | Unique ID - for HA internal identification
        device_id = entry.data.get(CONF_DEVICE_ID, "")
        self._attr_unique_id = f"{device_id}_{self._entity_id}"

        # 获取摄像头端口（默认 82）| Get camera port (default 82)
        self._camera_port = entity_config.get("port", DEFAULT_CAMERA_PORT)

        # 构建 URL | Build URLs
        self._stream_url = entity_config.get(
            "stream_url",
            f"http://{host}:{self._camera_port}/stream"
        )
        self._still_url = entity_config.get(
            "still_url",
            f"http://{host}:{self._camera_port}/camera"
        )

        # 设置图标 | Set icon
        self._attr_icon = entity_config.get("icon", "mdi:camera")

        # 帧间隔 - 静态图片刷新间隔
        # Frame interval - still image refresh interval
        self._attr_frame_interval = 0.25  # 4 fps | 4fps

        # 初始乐观可用，确保访问令牌立即写入实体属性
        # Optimistically available at start so the access token is written immediately
        self._attr_available = True

        # 摄像头初始化完成 | Camera initialization complete
        _LOGGER.info(
            "Camera initialized: %s (stream=%s)",
            self._attr_name,
            self._stream_url,
        )

    @property
    def device_info(self) -> DeviceInfo:
        """
        返回设备信息
        Return device info.
        """
        device_data = self._coordinator.device.device_info
        entry_data = self._entry.data
        
        # 获取设备 IP 地址 | Get device IP address
        host = entry_data.get("host", "")

        info = DeviceInfo(
            identifiers={(DOMAIN, entry_data.get(CONF_DEVICE_ID, ""))},
            name=device_data.get("name", "Seeed HA Device"),
            manufacturer=MANUFACTURER,
            model=entry_data.get(CONF_MODEL, device_data.get("model", "ESP32")),
            sw_version=device_data.get("version", "1.0.0"),
        )
        
        # 添加配置 URL（设备 IP 地址）| Add configuration URL (device IP)
        if host:
            info["configuration_url"] = f"http://{host}"
        
        # 添加 MAC 地址连接信息 | Add MAC address connection info
        mac_address = entry_data.get("mac_address", "")
        if mac_address:
            from homeassistant.helpers.device_registry import CONNECTION_NETWORK_MAC
            info["connections"] = {(CONNECTION_NETWORK_MAC, mac_address.lower())}
        
        return info

    async def async_added_to_hass(self) -> None:
        """
        实体加入 HA 后启动可达性探测
        Start reachability probing after the entity is added to HA.
        """
        await super().async_added_to_hass()

        # 立即探测一次，尽快反映真实可达性
        # Probe once right away to reflect real reachability quickly
        self.hass.async_create_task(self._async_update_availability())

        # 定时探测：即使没人观看，也保持可用性与访问令牌为最新
        # Periodic probe: keep availability and access token fresh even when unwatched
        self.async_on_remove(
            async_track_time_interval(
                self.hass,
                self._async_scheduled_availability_check,
                timedelta(seconds=CAMERA_AVAILABILITY_SCAN_INTERVAL),
            )
        )

    async def _async_scheduled_availability_check(
        self, now: datetime | None = None
    ) -> None:
        """
        定时可达性检查回调
        Scheduled availability check callback.

        参数 | Args:
            now: 定时器触发时间 | Timer fire time
        """
        await self._async_update_availability()

    async def _async_update_availability(self) -> None:
        """
        探测 HTTP 取图通道并按结果更新可用性
        Probe the HTTP image channel and update availability accordingly.
        """
        self._set_available(await self._async_probe_still_endpoint())

    async def _async_probe_still_endpoint(self) -> bool:
        """
        探测静态图片端点是否可达
        Probe whether the still image endpoint is reachable.

        返回 | Returns:
            bool: 端点是否返回 200 | Whether the endpoint returns 200
        """
        session = async_get_clientsession(self.hass)
        try:
            async with asyncio.timeout(5):
                async with session.get(self._still_url) as response:
                    return response.status == 200
        except (asyncio.TimeoutError, aiohttp.ClientError):
            return False
        except Exception as err:
            _LOGGER.debug("Camera availability probe error: %s", err)
            return False

    def _set_available(self, available: bool) -> None:
        """
        更新可用性，仅在变化时写入状态
        Update availability, writing state only when it changes.

        参数 | Args:
            available: 目标可用性 | Target availability
        """
        if available != self._attr_available:
            self._attr_available = available
            self.async_write_ha_state()

    async def async_camera_image(
        self, width: int | None = None, height: int | None = None
    ) -> bytes | None:
        """
        获取摄像头静态图片
        Return bytes of camera image.

        这个方法被 Home Assistant 调用来获取摄像头的静态图片。
        This method is called by Home Assistant to get still images from the camera.
        """
        session = async_get_clientsession(self.hass)
        
        try:
            async with asyncio.timeout(10):
                async with session.get(self._still_url) as response:
                    if response.status == 200:
                        self._set_available(True)
                        return await response.read()
                    else:
                        _LOGGER.warning(
                            "Failed to get camera image: HTTP %s",
                            response.status
                        )
        except asyncio.TimeoutError:
            _LOGGER.warning("Timeout getting camera image from %s", self._still_url)
        except aiohttp.ClientError as err:
            _LOGGER.warning("Error getting camera image: %s", err)
        except Exception as err:
            _LOGGER.error("Unexpected error getting camera image: %s", err)

        self._set_available(False)
        return None

    @property
    def extra_state_attributes(self) -> dict[str, Any]:
        """
        返回额外的状态属性
        Return extra state attributes.
        """
        return {
            "stream_url": self._stream_url,
            "still_image_url": self._still_url,
            "host": self._host,
            "port": self._camera_port,
        }

    async def stream_source(self) -> str | None:
        """
        返回流源 URL（当前不使用 MJPEG 流）
        Return the source of the stream (currently not using MJPEG stream).

        由于 ESP32 的 MJPEG 流与 Home Assistant 的 stream 组件不完全兼容，
        我们使用静态图片刷新模式代替。返回 None 禁用流功能。
        
        Since ESP32's MJPEG stream is not fully compatible with Home Assistant's
        stream component, we use still image refresh mode instead.
        Return None to disable streaming.
        """
        return None  # 禁用 MJPEG 流，使用静态图片刷新 | Disable MJPEG, use still image refresh

