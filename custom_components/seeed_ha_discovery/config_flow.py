"""
Seeed HA Discovery - 配置流程
Seeed HA Discovery - Config flow.

这个文件处理设备的添加流程，支持三种方式：
This file handles the device addition flow, supporting three methods:
1. 手动添加 - 用户输入设备 IP 地址 (WiFi 设备)
   Manual addition - User enters device IP address (WiFi devices)
2. 自动发现 (mDNS) - 通过 Zeroconf 自动发现局域网内的 WiFi 设备
   Auto-discovery (mDNS) - Discover WiFi devices on LAN via Zeroconf
3. 自动发现 (BLE) - 通过蓝牙发现 BTHome 格式的 BLE 设备
   Auto-discovery (BLE) - Discover BLE devices in BTHome format via Bluetooth

WiFi 自动发现工作原理 | WiFi auto-discovery mechanism:
- ESP32 设备会广播 mDNS 服务 (_seeed_ha._tcp)
  ESP32 devices broadcast mDNS service (_seeed_ha._tcp)
- Home Assistant 监听这个服务类型
  Home Assistant listens for this service type
- 当发现新设备时，会弹出通知让用户确认添加
  When new device is found, a notification pops up for user confirmation

BLE 自动发现工作原理 | BLE auto-discovery mechanism:
- XIAO nRF52840/ESP32 设备广播 BTHome 格式的数据
  XIAO nRF52840/ESP32 devices broadcast BTHome format data
- Home Assistant 通过蓝牙适配器扫描并识别
  Home Assistant scans and identifies via Bluetooth adapter
- 当发现新设备时，会弹出通知让用户确认添加
  When new device is found, a notification pops up for user confirmation
"""
from __future__ import annotations

import asyncio
import logging
from typing import Any

import aiohttp
import voluptuous as vol

from homeassistant import config_entries
from homeassistant.components import zeroconf
from homeassistant.components.bluetooth import BluetoothServiceInfoBleak
from homeassistant.const import CONF_HOST, CONF_NAME
from homeassistant.core import callback
from homeassistant.data_entry_flow import FlowResult
from homeassistant.helpers.aiohttp_client import async_get_clientsession
from homeassistant.helpers import selector

from .const import (
    DOMAIN,
    CONF_DEVICE_ID,
    CONF_PORT,
    CONF_MODEL,
    CONF_CONNECTION_TYPE,
    CONF_BLE_ADDRESS,
    CONF_BLE_CONTROL,
    CONF_BLE_SUBSCRIBED_ENTITIES,
    CONF_SUBSCRIBED_ENTITIES,
    CONNECTION_TYPE_WIFI,
    CONNECTION_TYPE_BLE,
    DEFAULT_HTTP_PORT,
    DEFAULT_WS_PORT,
    SEEED_CONTROL_SERVICE_UUID,
)
from .bluetooth import parse_ble_advertisement, is_seeed_ble_device

# 创建日志记录器 | Create logger
_LOGGER = logging.getLogger(__name__)


class SeeedHAConfigFlow(config_entries.ConfigFlow, domain=DOMAIN):
    """
    Seeed HA Discovery 配置流程处理器
    Handle a config flow for Seeed HA Discovery.

    这个类处理三种配置流程：
    1. user - 用户手动输入设备地址
    2. zeroconf - WiFi 设备自动发现后用户确认
    3. bluetooth - BLE 设备自动发现后用户确认
    """

    # 配置流程版本，用于迁移旧配置 | Config flow version for migration
    VERSION = 1

    @staticmethod
    @callback
    def async_get_options_flow(
        config_entry: config_entries.ConfigEntry,
    ) -> config_entries.OptionsFlow:
        """
        获取选项配置流程
        Get the options flow for this handler.
        
        用于让用户配置要订阅的 HA 实体。
        Used to let users configure HA entities to subscribe.
        """
        return SeeedHAOptionsFlow(config_entry)

    def __init__(self) -> None:
        """
        初始化配置流程
        Initialize the config flow.

        保存自动发现时获取的设备信息。
        Saves device info obtained during auto-discovery.
        """
        # 设备地址 (WiFi: IP 地址, BLE: MAC 地址) | Device address (WiFi: IP, BLE: MAC)
        self._host: str | None = None
        # WebSocket 端口 (仅 WiFi) | WebSocket port (WiFi only)
        self._port: int = DEFAULT_WS_PORT
        # 设备唯一 ID | Device unique ID
        self._device_id: str | None = None
        # 设备名称 | Device name
        self._device_name: str | None = None
        # 设备型号 | Device model
        self._model: str | None = None
        # 连接类型 (wifi / ble) | Connection type (wifi / ble)
        self._connection_type: str = CONNECTION_TYPE_WIFI
        # BLE 设备地址 | BLE device address
        self._ble_address: str | None = None
        # BLE 设备的传感器信息 (用于显示) | BLE sensor info (for display)
        self._ble_sensors: list[str] = []
        # BLE 设备是否支持控制 | Whether BLE device supports control
        self._ble_control: bool = False
        # BLE 设备的开关配置 | BLE switch configs
        self._switch_configs: list[dict[str, Any]] = []

    async def async_step_user(
        self, user_input: dict[str, Any] | None = None
    ) -> FlowResult:
        """
        处理用户手动添加设备
        Handle the initial step - manual device addition.

        显示一个表单让用户输入设备 IP 地址。
        Shows a form for user to enter device IP address.

        参数 | Args:
            user_input: 用户输入的数据，首次调用时为 None | User input data, None on first call

        返回 | Returns:
            FlowResult: 下一步的操作（显示表单或创建入口）| Next action (show form or create entry)
        """
        errors: dict[str, str] = {}

        if user_input is not None:
            # 用户已提交表单 | User submitted form
            host = user_input[CONF_HOST]
            port = user_input.get(CONF_PORT, DEFAULT_WS_PORT)

            # 尝试连接到设备 | Trying to connect to device
            _LOGGER.info("Trying to connect to device: %s:%s", host, port)

            # 尝试连接并获取设备信息 | Try to connect and get device info
            try:
                device_info = await self._async_get_device_info(host)

                if device_info:
                    # 成功获取设备信息 | Successfully got device info
                    device_id = device_info.get("device_id", host.replace(".", "_"))

                    # 检查设备是否已存在 | Check if device already exists
                    await self.async_set_unique_id(device_id)
                    self._abort_if_unique_id_configured()

                    # 设备信息获取成功 | Device info retrieved successfully
                    _LOGGER.info("Device info retrieved: %s", device_info)

                    # 创建配置入口 | Create config entry
                    return self.async_create_entry(
                        title=device_info.get("name", f"Seeed HA ({host})"),
                        data={
                            CONF_HOST: host,
                            CONF_PORT: port,
                            CONF_DEVICE_ID: device_id,
                            CONF_MODEL: device_info.get("model", "ESP32"),
                            CONF_CONNECTION_TYPE: CONNECTION_TYPE_WIFI,
                        },
                    )
                else:
                    # 无法获取设备信息 | Cannot get device info
                    _LOGGER.warning("Cannot get device info: %s", host)
                    errors["base"] = "cannot_connect"

            except asyncio.TimeoutError:
                # 连接超时 | Connection timeout
                _LOGGER.warning("Connection timeout: %s", host)
                errors["base"] = "timeout"
            except Exception as err:
                # 连接时发生未知错误 | Unknown error during connection
                _LOGGER.exception("Unknown error during connection: %s", err)
                errors["base"] = "unknown"

        # 显示输入表单 | Show input form
        return self.async_show_form(
            step_id="user",
            data_schema=vol.Schema(
                {
                    # 设备地址 - 必填 | Device address - required
                    vol.Required(CONF_HOST): str,
                    # WebSocket 端口 - 可选，默认 81 | WebSocket port - optional, default 81
                    vol.Optional(CONF_PORT, default=DEFAULT_WS_PORT): int,
                }
            ),
            errors=errors,
        )

    async def async_step_zeroconf(
        self, discovery_info: zeroconf.ZeroconfServiceInfo
    ) -> FlowResult:
        """
        处理 mDNS 自动发现 (WiFi 设备)
        Handle zeroconf discovery.

        当 HA 发现一个新的 Seeed HA WiFi 设备时，这个函数会被调用。
        设备通过广播 _seeed_ha._tcp 服务被发现。

        参数 | Args:
            discovery_info: mDNS 发现的设备信息 | mDNS discovered device info

        返回 | Returns:
            FlowResult: 跳转到确认步骤 | Jump to confirm step
        """
        # 发现 WiFi 设备 | Discovered WiFi device
        _LOGGER.info("Discovered Seeed HA WiFi device: %s", discovery_info)

        # 从发现信息中提取设备数据 | Extract device data from discovery info
        host = discovery_info.host
        port = discovery_info.port or DEFAULT_WS_PORT
        properties = discovery_info.properties

        # 从 mDNS TXT 记录中获取设备信息
        # Get device info from mDNS TXT records
        device_id = properties.get("id", host.replace(".", "_"))
        device_name = properties.get("name", f"Seeed HA ({host})")
        model = properties.get("model", "ESP32")

        # 设备信息 | Device info
        _LOGGER.info("Device ID: %s, Name: %s, Model: %s", device_id, device_name, model)

        # 设置唯一 ID，如果设备已配置则更新其地址 | Set unique ID, update address if configured
        await self.async_set_unique_id(device_id)
        self._abort_if_unique_id_configured(updates={CONF_HOST: host})

        # 保存发现的设备信息 | Save discovered device info
        self._host = host
        self._port = port
        self._device_id = device_id
        self._device_name = device_name
        self._model = model
        self._connection_type = CONNECTION_TYPE_WIFI

        # 设置通知中显示的设备名称 | Set device name shown in notification
        self.context["title_placeholders"] = {"name": device_name}

        # 跳转到确认步骤 | Jump to confirm step
        return await self.async_step_zeroconf_confirm()

    async def async_step_zeroconf_confirm(
        self, user_input: dict[str, Any] | None = None
    ) -> FlowResult:
        """
        用户确认添加自动发现的 WiFi 设备
        Handle user confirmation of discovered WiFi device.

        显示设备信息，让用户确认是否添加。
        Shows device info and asks user to confirm addition.

        参数 | Args:
            user_input: 用户确认后为空字典，取消为 None | Empty dict after confirm, None if cancelled

        返回 | Returns:
            FlowResult: 创建配置入口或显示确认表单 | Create entry or show confirm form
        """
        if user_input is not None:
            # 用户点击了确认，创建配置入口 | User confirmed, create config entry
            _LOGGER.info("User confirmed adding WiFi device: %s", self._device_name)

            return self.async_create_entry(
                title=self._device_name or f"Seeed HA ({self._host})",
                data={
                    CONF_HOST: self._host,
                    CONF_PORT: self._port,
                    CONF_DEVICE_ID: self._device_id,
                    CONF_MODEL: self._model,
                    CONF_CONNECTION_TYPE: CONNECTION_TYPE_WIFI,
                },
            )

        # 显示确认表单 | Show confirm form
        return self.async_show_form(
            step_id="zeroconf_confirm",
            description_placeholders={
                "name": self._device_name,
                "host": self._host,
                "model": self._model,
            },
        )

    # =========================================================================
    # BLE 设备发现流程 | BLE Device Discovery Flow
    # =========================================================================

    async def async_step_bluetooth(
        self, discovery_info: BluetoothServiceInfoBleak
    ) -> FlowResult:
        """
        处理蓝牙自动发现 (BLE 设备)
        Handle Bluetooth discovery.

        当 HA 发现一个新的 Seeed HA BLE 设备时，这个函数会被调用。
        设备通过广播 BTHome 格式的数据被发现。

        参数 | Args:
            discovery_info: 蓝牙发现的设备信息 | Bluetooth discovered device info

        返回 | Returns:
            FlowResult: 跳转到确认步骤 | Jump to confirm step
        """
        _LOGGER.info("Discovered Seeed HA BLE device: %s (%s)", 
                     discovery_info.name, discovery_info.address)

        # 检查是否是有效的 Seeed/BTHome 设备 | Check if valid Seeed/BTHome device
        if not is_seeed_ble_device(discovery_info):
            _LOGGER.debug("Not a valid Seeed BLE device")
            return self.async_abort(reason="not_supported")

        # 解析 BLE 广播数据 | Parse BLE advertisement data
        device = parse_ble_advertisement(discovery_info)
        if device is None:
            _LOGGER.debug("Cannot parse BLE advertisement data")
            return self.async_abort(reason="not_supported")

        # 使用 BLE 地址作为唯一 ID | Use BLE address as unique ID
        device_id = f"ble_{discovery_info.address.replace(':', '_').lower()}"

        # 设置唯一 ID | Set unique ID
        await self.async_set_unique_id(device_id)
        self._abort_if_unique_id_configured()

        # 保存发现的设备信息 | Save discovered device info
        self._ble_address = discovery_info.address
        self._device_id = device_id
        self._device_name = device.name
        self._model = "XIAO BLE"  # 默认型号 | Default model
        self._connection_type = CONNECTION_TYPE_BLE

        # 检查是否支持控制服务 | Check if control service is supported
        # 只有在广播中明确包含控制服务 UUID 的设备才启用控制
        # Only enable control if device explicitly advertises control service UUID
        # 纯传感器设备（如 ButtonBLE、TemperatureBLE）不会广播此 UUID
        # Pure sensor devices (like ButtonBLE, TemperatureBLE) don't advertise this UUID
        self._ble_control = False
        if discovery_info.service_uuids:
            for uuid in discovery_info.service_uuids:
                if SEEED_CONTROL_SERVICE_UUID.lower() in uuid.lower():
                    self._ble_control = True
                    _LOGGER.info("BLE device supports control service: %s", self._ble_address)
                    break
        
        if not self._ble_control:
            _LOGGER.info("BLE device supports sensor only: %s", self._ble_address)

        # 保存传感器信息用于显示 | Save sensor info for display
        self._ble_sensors = [
            f"{s.name}: {s.value} {s.unit or ''}" for s in device.sensors
        ]

        # 如果支持控制，根据二进制传感器生成开关配置
        # If control is supported, generate switch configs based on binary sensors
        switch_configs = []
        if self._ble_control:
            binary_sensors = [s for s in device.sensors if s.device_class in [None, "power", "opening"]]
            if binary_sensors:
                # 默认开关名称 | Default switch names
                switch_names = ["Click", "Double Click", "Long Press"]
                for i, sensor in enumerate(binary_sensors):
                    name = switch_names[i] if i < len(switch_names) else f"Switch {i+1}"
                    switch_configs.append({
                        "index": i,
                        "name": name,
                        "id": f"switch_{i}",
                    })
                _LOGGER.info("Generated %d switch configs", len(switch_configs))
        
        self._switch_configs = switch_configs

        # 设置通知中显示的设备名称 | Set device name shown in notification
        self.context["title_placeholders"] = {"name": device.name}

        # 跳转到确认步骤 | Jump to confirm step
        return await self.async_step_bluetooth_confirm()

    async def async_step_bluetooth_confirm(
        self, user_input: dict[str, Any] | None = None
    ) -> FlowResult:
        """
        用户确认添加自动发现的 BLE 设备
        Handle user confirmation of discovered BLE device.

        显示设备信息和当前传感器数据，让用户确认是否添加。
        Shows device info and current sensor data, asks user to confirm addition.

        参数 | Args:
            user_input: 用户确认后为空字典，取消为 None | Empty dict after confirm, None if cancelled

        返回 | Returns:
            FlowResult: 创建配置入口或显示确认表单 | Create entry or show confirm form
        """
        if user_input is not None:
            # 用户点击了确认，创建配置入口 | User confirmed, create config entry
            _LOGGER.info("User confirmed adding BLE device: %s (%s), control=%s", 
                         self._device_name, self._ble_address, self._ble_control)

            entry_data = {
                CONF_DEVICE_ID: self._device_id,
                CONF_BLE_ADDRESS: self._ble_address,
                CONF_MODEL: self._model,
                CONF_CONNECTION_TYPE: CONNECTION_TYPE_BLE,
                CONF_BLE_CONTROL: self._ble_control,
            }
            
            # 如果有开关配置，保存它 | If there are switch configs, save them
            if hasattr(self, '_switch_configs') and self._switch_configs:
                entry_data["switch_configs"] = self._switch_configs
                _LOGGER.info("Saving switch configs: %s", self._switch_configs)
            
            return self.async_create_entry(
                title=self._device_name or f"Seeed BLE ({self._ble_address})",
                data=entry_data,
            )

        # 格式化传感器信息用于显示 | Format sensor info for display
        sensors_str = ", ".join(self._ble_sensors) if self._ble_sensors else "None"
        control_str = "Yes (supports switch control)" if self._ble_control else "No (sensor only)"

        # 显示确认表单 | Show confirm form
        return self.async_show_form(
            step_id="bluetooth_confirm",
            description_placeholders={
                "name": self._device_name,
                "address": self._ble_address,
                "model": self._model,
                "sensors": sensors_str,
                "control": control_str,
            },
        )

    # =========================================================================
    # 辅助方法 | Helper Methods
    # =========================================================================

    async def _async_get_device_info(self, host: str) -> dict[str, Any] | None:
        """
        通过 HTTP 获取 WiFi 设备信息
        Get WiFi device info via HTTP.

        ESP32 设备在 /info 端点提供 JSON 格式的设备信息。
        ESP32 device provides JSON device info at /info endpoint.

        参数 | Args:
            host: 设备 IP 地址 | Device IP address

        返回 | Returns:
            dict: 设备信息字典，失败时返回 None | Device info dict, None on failure
        """
        session = async_get_clientsession(self.hass)
        url = f"http://{host}:{DEFAULT_HTTP_PORT}/info"

        # 请求设备信息 | Request device info
        _LOGGER.debug("Requesting device info: %s", url)

        try:
            async with asyncio.timeout(10):
                async with session.get(url) as response:
                    if response.status == 200:
                        data = await response.json()
                        _LOGGER.debug("Device info: %s", data)
                        return data
                    else:
                        # 设备返回错误状态码 | Device returned error status
                        _LOGGER.warning("Device returned error status: %s", response.status)
                        return None
        except aiohttp.ClientError as err:
            # HTTP 请求失败 | HTTP request failed
            _LOGGER.warning("HTTP request failed: %s", err)
            return None
        except asyncio.TimeoutError:
            # HTTP 请求超时 | HTTP request timeout
            _LOGGER.warning("HTTP request timeout")
            raise


# =============================================================================
# 选项配置流程 | Options Flow
# =============================================================================

# BLE 设备最大订阅实体数 | Max subscribed entities for BLE device
BLE_MAX_SUBSCRIBED_ENTITIES = 16


class SeeedHAOptionsFlow(config_entries.OptionsFlow):
    """
    Seeed HA Discovery 选项配置流程
    Handle options flow for Seeed HA Discovery.

    让用户选择要下发到设备的 Home Assistant 实体。
    Allows users to select HA entities to push to the device.
    """

    def __init__(self, config_entry: config_entries.ConfigEntry) -> None:
        """
        初始化选项配置流程
        Initialize options flow.
        """
        self.config_entry = config_entry

    async def async_step_init(
        self, user_input: dict[str, Any] | None = None
    ) -> FlowResult:
        """
        显示实体选择界面
        Show entity selection interface.

        让用户选择要订阅并推送到 Arduino 设备的 HA 实体。
        Allows users to select HA entities to subscribe and push to Arduino device.
        """
        connection_type = self.config_entry.data.get(
            CONF_CONNECTION_TYPE, CONNECTION_TYPE_WIFI
        )

        # 根据连接类型选择不同的配置流程
        # Choose different config flow based on connection type
        if connection_type == CONNECTION_TYPE_BLE:
            return await self.async_step_ble_entities(user_input)
        else:
            return await self.async_step_wifi_entities(user_input)

    async def async_step_wifi_entities(
        self, user_input: dict[str, Any] | None = None
    ) -> FlowResult:
        """
        WiFi 设备的实体选择界面
        Entity selection interface for WiFi devices.
        """
        if user_input is not None:
            # 用户保存了选择 | User saved selection
            _LOGGER.info(
                "User selected entities to subscribe (WiFi): %s",
                user_input.get(CONF_SUBSCRIBED_ENTITIES, [])
            )
            return self.async_create_entry(
                title="",
                data={
                    CONF_SUBSCRIBED_ENTITIES: user_input.get(CONF_SUBSCRIBED_ENTITIES, [])
                },
            )

        # 获取当前已选择的实体 | Get currently selected entities
        current_entities = self.config_entry.options.get(CONF_SUBSCRIBED_ENTITIES, [])

        # 显示实体选择表单 | Show entity selection form
        return self.async_show_form(
            step_id="wifi_entities",
            data_schema=vol.Schema(
                {
                    vol.Optional(
                        CONF_SUBSCRIBED_ENTITIES,
                        default=current_entities,
                    ): selector.EntitySelector(
                        selector.EntitySelectorConfig(
                            # 允许选择多个实体 | Allow multiple selection
                            multiple=True,
                            # 可以选择的实体域 | Allowed entity domains
                            domain=["sensor", "binary_sensor", "switch", "light", "climate", "weather"],
                        )
                    ),
                }
            ),
            description_placeholders={
                "device_name": self.config_entry.title,
            },
        )

    async def async_step_ble_entities(
        self, user_input: dict[str, Any] | None = None
    ) -> FlowResult:
        """
        BLE 设备的实体选择界面
        Entity selection interface for BLE devices.
        
        BLE 设备需要按索引配置实体，最多支持 16 个。
        BLE devices need entities configured by index, up to 16.
        """
        # 注意：不再严格检查 CONF_BLE_CONTROL，因为 128 位 UUID 可能在发现时未被检测到
        # 如果设备实际不支持，推送会静默失败，不会影响其他功能
        # Note: No longer strictly checking CONF_BLE_CONTROL as 128-bit UUID may not be detected during discovery
        # If device doesn't actually support it, push will fail silently without affecting other features
        _LOGGER.debug(
            "BLE entity config for %s, control flag: %s",
            self.config_entry.title,
            self.config_entry.data.get(CONF_BLE_CONTROL, "not set")
        )

        if user_input is not None:
            # 将选择的实体列表转换为索引映射
            # Convert selected entity list to index mapping
            entities = user_input.get(CONF_BLE_SUBSCRIBED_ENTITIES, [])
            entity_map = {}
            for i, entity_id in enumerate(entities[:BLE_MAX_SUBSCRIBED_ENTITIES]):
                if entity_id:  # 跳过空值 | Skip empty values
                    entity_map[str(i)] = entity_id

            _LOGGER.info(
                "User selected entities to subscribe (BLE): %s",
                entity_map
            )

            # 同时更新 data 和 options
            # Update both data and options
            # 注意：BLE 订阅实体存储在 data 中，因为需要在设备管理器初始化时使用
            # Note: BLE subscribed entities stored in data because needed at manager init
            new_data = dict(self.config_entry.data)
            new_data[CONF_BLE_SUBSCRIBED_ENTITIES] = entity_map
            # 如果用户配置了实体，说明设备应该支持控制
            # If user configured entities, device should support control
            if entity_map:
                new_data[CONF_BLE_CONTROL] = True
            self.hass.config_entries.async_update_entry(
                self.config_entry,
                data=new_data,
            )

            # 更新 data 后需要手动触发重载，因为 options 监听器不会响应 data 变化
            # After updating data, need to manually trigger reload since options listener doesn't respond to data changes
            entry_id = self.config_entry.entry_id
            _LOGGER.info("Scheduling delayed reload for BLE entry %s after entity config", entry_id)
            
            async def delayed_reload():
                """延迟重载以避免中断正在进行的操作"""
                _LOGGER.info("Waiting 3s before reload to ensure config is persisted...")
                await asyncio.sleep(3.0)  # Wait for config persistence and old instance cleanup
                _LOGGER.info("Now reloading entry %s", entry_id)
                await self.hass.config_entries.async_reload(entry_id)
            
            self.hass.async_create_task(delayed_reload())

            return self.async_create_entry(
                title="",
                data={
                    CONF_BLE_SUBSCRIBED_ENTITIES: entity_map,
                },
            )

        # 获取当前已选择的实体 | Get currently selected entities
        current_map = self.config_entry.data.get(CONF_BLE_SUBSCRIBED_ENTITIES, {})
        # 转换为列表格式（按索引排序） | Convert to list format (sorted by index)
        current_entities = []
        for i in range(BLE_MAX_SUBSCRIBED_ENTITIES):
            entity_id = current_map.get(str(i), "")
            if entity_id:
                current_entities.append(entity_id)

        # 显示实体选择表单 | Show entity selection form
        return self.async_show_form(
            step_id="ble_entities",
            data_schema=vol.Schema(
                {
                    vol.Optional(
                        CONF_BLE_SUBSCRIBED_ENTITIES,
                        default=current_entities,
                    ): selector.EntitySelector(
                        selector.EntitySelectorConfig(
                            # 允许选择多个实体 | Allow multiple selection
                            multiple=True,
                            # 可以选择的实体域 | Allowed entity domains
                            domain=["sensor", "binary_sensor", "switch", "light", "climate", "weather"],
                        )
                    ),
                }
            ),
            description_placeholders={
                "device_name": self.config_entry.title,
                "max_entities": str(BLE_MAX_SUBSCRIBED_ENTITIES),
            },
        )
