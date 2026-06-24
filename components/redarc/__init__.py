import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import canbus
from esphome.components.canbus import CONF_BIT_RATE, CONF_CAN_ID, CONF_USE_EXTENDED_ID
from esphome.components.esp32_can import canbus as esp32_can
from esphome.components.homeassistant import time as homeassistant_time
from esphome.const import CONF_ID, CONF_PLATFORM, CONF_RX_PIN, CONF_TX_PIN
from esphome.core import CORE

from . import (
    _battery_sensor,
    _manager,
    _redvision_display,
    _tvms_1280,
    _tvms_rogue,
)

CODEOWNERS = ["@dev8edss"]
DEPENDENCIES = ["canbus"]
AUTO_LOAD = [
    "binary_sensor",
    "button",
    "canbus",
    "esp32_can",
    "homeassistant",
    "light",
    "number",
    "select",
    "sensor",
    "switch",
    "text_sensor",
    "time",
]

CONF_CANBUS = "canbus"
CONF_CANBUS_ID = "canbus_id"
CONF_TIME = "time"
CONF_HOST_ADDRESS = "host_address"
CONF_DISCOVERY_DELAY = "discovery_delay"
CONF_FILTER_INTERVAL = "filter_interval"
CONF_HISTORY_POLL_INTERVAL = "history_poll_interval"
CONF_TRANSITION_LENGTH = "transition_length"
CONF_SOURCE_ADDRESS = "source_address"

# Nested device keys. Every device type is optional and accepts either a single
# entry or a list, so multiple of the same device (on different source
# addresses) can be added.
CONF_BATTERY_SENSOR = "battery_sensor"
CONF_MANAGER = "manager"
CONF_REDVISION_DISPLAY = "redvision_display"
CONF_TVMS_ROGUE = "tvms_rogue"
CONF_TVMS_1280 = "tvms_1280"

DEVICE_KEYS = (
    CONF_BATTERY_SENSOR,
    CONF_MANAGER,
    CONF_REDVISION_DISPLAY,
    CONF_TVMS_ROGUE,
    CONF_TVMS_1280,
)


def zero_or_positive_time_period_milliseconds(value):
    if value in (0, "0", "0s", "0ms"):
        return 0
    return cv.positive_time_period_milliseconds(value)


def _validate_unique_source_addresses(config):
    # Two devices answering on the same source address would alias on the bus,
    # so every configured device must use a distinct source_address.
    seen = {}
    for key in DEVICE_KEYS:
        for device in config.get(key) or []:
            source_address = device[CONF_SOURCE_ADDRESS]
            if source_address in seen:
                raise cv.Invalid(
                    f"source_address 0x{source_address:02X} is used by both "
                    f"'{seen[source_address]}' and '{device[CONF_ID]}'; each REDARC "
                    f"device must have a unique source_address"
                )
            seen[source_address] = device[CONF_ID]
    return config

ns = cg.esphome_ns.namespace("redarc_common")
RedarcCommonComponent = ns.class_("RedarcCommonComponent", cg.Component)

# The CAN bus is set up inside the component instead of a top-level canbus:
# block, reusing esp32_can's own platform schema (so every option — bit_rate,
# mode, rx_queue_len, … — matches ESPHome exactly and is validated by esp32_can).
# We supply REDARC-friendly defaults by injecting them as raw input before
# esp32_can validates, rather than overriding its validators (which is version-
# fragile). Defaults are only injected for keys the installed esp32_can has, so
# this works across ESPHome versions. For an SPI transceiver such as mcp2515,
# declare a normal top-level canbus: block and point `canbus_id:` at it instead.
_CANBUS_DEFAULTS = {
    CONF_TX_PIN: "GPIO22",
    CONF_RX_PIN: "GPIO19",
    CONF_BIT_RATE: "250KBPS",
    CONF_CAN_ID: 0x7FE,
    CONF_USE_EXTENDED_ID: True,
    "rx_queue_len": 64,
}
try:
    _ESP32_CAN_KEYS = {
        marker.schema
        for marker in esp32_can.CONFIG_SCHEMA.schema
        if isinstance(getattr(marker, "schema", None), str)
    }
except (AttributeError, TypeError):
    # Fall back to the keys present in every esp32_can version (skip rx_queue_len,
    # which is newer) if the schema can't be introspected.
    _ESP32_CAN_KEYS = {
        CONF_TX_PIN, CONF_RX_PIN, CONF_BIT_RATE, CONF_CAN_ID, CONF_USE_EXTENDED_ID,
    }


def _apply_canbus_defaults(value):
    value = dict(value or {})
    for key, default in _CANBUS_DEFAULTS.items():
        if key in _ESP32_CAN_KEYS:
            value.setdefault(key, default)
    return value


CANBUS_SCHEMA = cv.All(_apply_canbus_defaults, esp32_can.CONFIG_SCHEMA)

# Likewise the Home Assistant time source (used by the Manager Set Time button)
# is created inside the component, reusing the homeassistant time schema. Time is
# enabled by default; `time: false` disables it (and the Set Time button). An
# empty `time:` / `time: {}` is the same as the default.
def _time_schema(value):
    if value is False:
        return False
    return homeassistant_time.CONFIG_SCHEMA({} if value is None else value)

CONFIG_SCHEMA = cv.All(
    cv.Schema({
        cv.GenerateID(): cv.declare_id(RedarcCommonComponent),
        cv.Optional(CONF_CANBUS): CANBUS_SCHEMA,
        cv.Optional(CONF_CANBUS_ID): cv.use_id(canbus.CanbusComponent),
        cv.Optional(CONF_TIME, default={}): _time_schema,
        cv.Optional(CONF_HOST_ADDRESS, default=0xFF): cv.hex_uint8_t,
        cv.Optional(CONF_DISCOVERY_DELAY, default="2000ms"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_FILTER_INTERVAL, default="5s"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_HISTORY_POLL_INTERVAL, default="60s"): zero_or_positive_time_period_milliseconds,
        cv.Optional(CONF_TRANSITION_LENGTH, default="0s"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_BATTERY_SENSOR): cv.ensure_list(_battery_sensor.SCHEMA),
        cv.Optional(CONF_MANAGER): cv.ensure_list(_manager.SCHEMA),
        cv.Optional(CONF_REDVISION_DISPLAY): cv.ensure_list(_redvision_display.SCHEMA),
        cv.Optional(CONF_TVMS_ROGUE): cv.ensure_list(_tvms_rogue.SCHEMA),
        cv.Optional(CONF_TVMS_1280): cv.ensure_list(_tvms_1280.SCHEMA),
    }).extend(cv.COMPONENT_SCHEMA),
    cv.has_exactly_one_key(CONF_CANBUS, CONF_CANBUS_ID),
    _validate_unique_source_addresses,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    # Compile in ESPHome's api on-client-connected trigger so the component can
    # re-scan for devices when any API client connects (incl. the dashboard log
    # viewer), without the user adding an `on_client_connected:` automation. The
    # trigger member and the code that fires it both sit behind this macro; the
    # component attaches an action to get_client_connected_trigger() in setup().
    if "api" in CORE.config:
        cg.add_define("USE_API_CLIENT_CONNECTED_TRIGGER")

    # CAN bus: either build the esp32_can interface from the nested `canbus:`
    # block, or reference an externally-declared one (e.g. mcp2515) via
    # `canbus_id:`.
    if CONF_CANBUS in config:
        await esp32_can.to_code(config[CONF_CANBUS])
        can = await cg.get_variable(config[CONF_CANBUS][CONF_ID])
    else:
        can = await cg.get_variable(config[CONF_CANBUS_ID])
    cg.add(var.set_canbus(can))

    # Home Assistant time source (enabled by default; `time: false` disables it
    # and the Manager Set Time button). Created inside the component.
    time_id = None
    if config[CONF_TIME] is not False:
        time_conf = config[CONF_TIME]
        await homeassistant_time.to_code(time_conf)
        # The homeassistant time platform's C++ sources are only copied into the
        # build when a `time:` platform entry exists. We create the time without
        # such an entry, so register one here so api_connection.cpp can find
        # homeassistant_time.h (USE_HOMEASSISTANT_TIME is defined by to_code).
        time_platform_entry = dict(time_conf)
        time_platform_entry[CONF_PLATFORM] = "homeassistant"
        CORE.config.setdefault("time", []).append(time_platform_entry)
        time_id = time_conf[CONF_ID]

    host_address = config[CONF_HOST_ADDRESS]
    cg.add(var.set_host_address(host_address))
    cg.add(var.set_discovery_delay_ms(config[CONF_DISCOVERY_DELAY]))

    filter_interval = config[CONF_FILTER_INTERVAL]
    history_poll = config[CONF_HISTORY_POLL_INTERVAL]
    transition_length = config[CONF_TRANSITION_LENGTH]

    # Devices inherit the top-level filter_interval (all devices), host_address
    # (command-sending devices), history_poll_interval (the SOC/solar poll
    # intervals), and transition_length (the Rogue light transition) unless they
    # explicitly override them, so each only has to be configured once.
    def _inherit(device_config, host=False, soc_history=False, solar_history=False, transition=False):
        device_config.setdefault(CONF_FILTER_INTERVAL, filter_interval)
        if host:
            device_config.setdefault(CONF_HOST_ADDRESS, host_address)
        if soc_history:
            device_config.setdefault(_battery_sensor.CONF_SOC_HISTORY_POLL_INTERVAL, history_poll)
        if solar_history:
            device_config.setdefault(_manager.CONF_SOLAR_HISTORY_POLL_INTERVAL, history_poll)
        if transition:
            device_config.setdefault(_tvms_rogue.CONF_TRANSITION_LENGTH, transition_length)
        return device_config

    # Battery sensors are built first so managers can reference them by id.
    for device_config in config.get(CONF_BATTERY_SENSOR) or []:
        await _battery_sensor.to_code(_inherit(device_config, host=True, soc_history=True))
    for device_config in config.get(CONF_MANAGER) or []:
        if time_id is not None:
            device_config.setdefault(_manager.CONF_TIME_ID, time_id)
        await _manager.to_code(_inherit(device_config, host=True, solar_history=True))
    if config.get(CONF_REDVISION_DISPLAY):
        for device_config in config[CONF_REDVISION_DISPLAY]:
            _inherit(device_config)
        await _redvision_display.to_code(config[CONF_REDVISION_DISPLAY])
    for device_config in config.get(CONF_TVMS_ROGUE) or []:
        await _tvms_rogue.to_code(_inherit(device_config, host=True, transition=True))
    for device_config in config.get(CONF_TVMS_1280) or []:
        await _tvms_1280.to_code(_inherit(device_config, host=True))
