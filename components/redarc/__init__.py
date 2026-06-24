import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import canbus
from esphome.components.canbus import CONF_BIT_RATE, CONF_CAN_ID, CONF_USE_EXTENDED_ID
from esphome.components.esp32_can import canbus as esp32_can
from esphome.components.homeassistant import time as homeassistant_time
from esphome.const import CONF_ID

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
# block, reusing esp32_can's own platform schema (so every option matches
# ESPHome exactly) with REDARC-friendly defaults for the bus constants.
# For an SPI transceiver such as mcp2515, declare a normal top-level canbus:
# block instead and point `canbus_id:` at it (see the README).
CANBUS_SCHEMA = esp32_can.CONFIG_SCHEMA.extend({
    cv.Optional(CONF_CAN_ID, default=0x7FE): cv.int_range(min=0, max=0x1FFFFFFF),
    cv.Optional(CONF_USE_EXTENDED_ID, default=True): cv.boolean,
    cv.Optional(CONF_BIT_RATE, default="250KBPS"): cv.enum(esp32_can.CAN_SPEEDS, upper=True),
})

# Likewise the Home Assistant time source (used by the Manager Set Time button)
# can be created inside the component, reusing the homeassistant time schema.
# Accept an empty `time:` (None) as well as `time: {}`.
def _time_schema(value):
    return homeassistant_time.CONFIG_SCHEMA({} if value is None else value)

CONFIG_SCHEMA = cv.All(
    cv.Schema({
        cv.GenerateID(): cv.declare_id(RedarcCommonComponent),
        cv.Optional(CONF_CANBUS): CANBUS_SCHEMA,
        cv.Optional(CONF_CANBUS_ID): cv.use_id(canbus.CanbusComponent),
        cv.Optional(CONF_TIME): _time_schema,
        cv.Optional(CONF_HOST_ADDRESS, default=0x22): cv.hex_uint8_t,
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

    # CAN bus: either build the esp32_can interface from the nested `canbus:`
    # block, or reference an externally-declared one (e.g. mcp2515) via
    # `canbus_id:`.
    if CONF_CANBUS in config:
        await esp32_can.to_code(config[CONF_CANBUS])
        can = await cg.get_variable(config[CONF_CANBUS][CONF_ID])
    else:
        can = await cg.get_variable(config[CONF_CANBUS_ID])
    cg.add(var.set_canbus(can))

    # Optional internal Home Assistant time source for the Manager Set Time button.
    time_var = None
    if CONF_TIME in config:
        await homeassistant_time.to_code(config[CONF_TIME])
        time_var = await cg.get_variable(config[CONF_TIME][CONF_ID])

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
        manager = await _manager.to_code(_inherit(device_config, host=True, solar_history=True))
        if time_var is not None and _manager.CONF_TIME_ID not in device_config:
            cg.add(manager.set_time_source(time_var))
    if config.get(CONF_REDVISION_DISPLAY):
        for device_config in config[CONF_REDVISION_DISPLAY]:
            _inherit(device_config)
        await _redvision_display.to_code(config[CONF_REDVISION_DISPLAY])
    for device_config in config.get(CONF_TVMS_ROGUE) or []:
        await _tvms_rogue.to_code(_inherit(device_config, host=True, transition=True))
    for device_config in config.get(CONF_TVMS_1280) or []:
        await _tvms_1280.to_code(_inherit(device_config, host=True))
