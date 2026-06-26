import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import canbus
from esphome.components.canbus import CONF_BIT_RATE, CONF_CAN_ID, CONF_USE_EXTENDED_ID
from esphome.components.esp32_can import canbus as esp32_can
from esphome.components.homeassistant import time as homeassistant_time
from esphome.const import CONF_ID, CONF_PLATFORM, CONF_RX_PIN, CONF_TX_PIN
from esphome.core import CORE, ID

from . import (
    _battery_sensor,
    _manager,
    _redvision_display,
    _tvms_1280,
    _tvms_rogue,
    _tvms_rogue_emulator,
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
CONF_FILTER_INTERVAL = "filter_interval"
CONF_HISTORY_POLL_INTERVAL = "history_poll_interval"
CONF_SOURCE_ADDRESS = "source_address"

# Nested device keys. Every device type is optional and accepts either a single
# entry or a list, so multiple of the same device (on different source
# addresses) can be added.
CONF_BATTERY_SENSOR = "battery_sensor"
CONF_MANAGER = "manager"
CONF_REDVISION_DISPLAY = "redvision_display"
CONF_TVMS_ROGUE = "tvms_rogue"
CONF_TVMS_ROGUE_EMULATOR = "tvms_rogue_emulator"
CONF_TVMS_1280 = "tvms_1280"

DEVICE_KEYS = (
    CONF_BATTERY_SENSOR,
    CONF_MANAGER,
    CONF_REDVISION_DISPLAY,
    CONF_TVMS_ROGUE,
    CONF_TVMS_ROGUE_EMULATOR,
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


# Explicit id-suffix overrides so every entity's derived object id matches its
# Home Assistant name (so id(...) references and the HA entity_id line up). The
# value is the slug of the entity's friendly name; keyed by the schema key.
# Entities whose key already slugifies to their name don't need an entry.
_ID_OVERRIDES = {
    # Manager30
    "source_device_current_id": "device_current",      # "Device Current"
    "solar_energy_id": "solar_energy_total",            # "Solar Energy Total"
    "solar_today_id": "solar_energy_today",             # "Solar Energy Today"
    "solar_day_history_id": "solar_day_1_12_history",   # "Solar Day 1-12 History"
    "clock_date_id": "can_date",                        # "CAN Date"
    "clock_time_id": "can_time",                        # "CAN Time"
    "clock_datetime_id": "can_date_time",               # "CAN Date Time"
    "set_clock_button_id": "set_time",                  # "Set Time"
    "clear_solar_history_button_id": "delete_solar_history",  # "Delete Solar History"
    # Battery
    "soc_calibration_button_id": "calibrate_soc_full",         # "Calibrate SOC Full"
    "clear_hourly_soc_button_id": "delete_hourly_soc_history", # "Delete Hourly SOC History"
    "clear_daily_soc_button_id": "delete_daily_soc_history",   # "Delete Daily SOC History"
    # TVMS Rogue
    "tank1_id": "tank_1",                              # "Tank 1"
    "tank2_id": "tank_2",                              # "Tank 2"
    # TVMS1280
    "temp1_id": "temperature_1",                       # "Temperature 1"
    "temp2_id": "temperature_2",                       # "Temperature 2"
    "voltage_input1_id": "voltage_input_1",            # "Voltage Input 1"
    "voltage_input2_id": "voltage_input_2",            # "Voltage Input 2"
}
# Rogue indexed entities: output level sensors and the dimmable light states.
for _i in range(1, 11):
    _ID_OVERRIDES[f"level_sensor_{_i}"] = f"output_{_i}_level"  # "Output N Level"
    _ID_OVERRIDES[f"light_state_{_i}"] = f"output_{_i}"         # "Output N" (light)
# TVMS1280 output switches are declared 0-indexed (output_0_id..output_9_id) but
# named "Output 1".."Output 10"; map the id to the 1-based name so
# TVMS1280_output_1 is "Output 1" (channel 0x04), not "Output 2".
for _i in range(10):
    _ID_OVERRIDES[f"output_{_i}_id"] = f"output_{_i + 1}"


def _derive_entity_ids(config):
    """Give every auto-generated sub-entity of a device a stable id derived from
    the device block's id (e.g. id: Manager30 -> Manager30_solar_power), so the
    entities can be referenced directly from user YAML, e.g.

        id(TVMS_Rogue_input_8).state
        id(TVMS1280_output_1).turn_on();

    The id suffix is the entity's HA name slug: _ID_OVERRIDES is consulted first,
    otherwise it's the schema key (trailing "_id" and entity-kind word stripped).
    Only runs when the device block has an explicit id:; otherwise the auto ids
    are kept.
    """
    parent = config.get(CONF_ID)
    if parent is None or parent.id is None:
        return config
    prefix = parent.id
    for key, value in config.items():
        if key == CONF_ID:
            continue
        if isinstance(value, ID) and value.is_declaration and not value.is_manual:
            suffix = _ID_OVERRIDES.get(key)
            if suffix is None:
                suffix = key[:-3] if key.endswith("_id") else key
                # Drop the entity-kind word from the end so ids read naturally
                # (charging_mode_select -> charging_mode, set_clock_button -> set_clock).
                for kind in ("_select", "_number", "_button"):
                    if suffix.endswith(kind):
                        suffix = suffix[: -len(kind)]
                        break
                # Normalise physical inputs to <device>_input_N on both devices.
                if suffix.startswith("button_sensor_"):
                    suffix = "input_" + suffix[len("button_sensor_"):]
                elif suffix.startswith("digital_input_"):
                    suffix = "input_" + suffix[len("digital_input_"):]
            value.id = f"{prefix}_{suffix}"
            value.is_manual = True
    return config


def _device_list(schema):
    return cv.ensure_list(cv.All(schema, _derive_entity_ids))


CONFIG_SCHEMA = cv.All(
    cv.Schema({
        cv.GenerateID(): cv.declare_id(RedarcCommonComponent),
        cv.Optional(CONF_CANBUS): CANBUS_SCHEMA,
        cv.Optional(CONF_CANBUS_ID): cv.use_id(canbus.CanbusComponent),
        cv.Optional(CONF_TIME, default={}): _time_schema,
        cv.Optional(CONF_HOST_ADDRESS, default=0xFF): cv.hex_uint8_t,
        cv.Optional(CONF_FILTER_INTERVAL, default="5s"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_HISTORY_POLL_INTERVAL, default="60s"): zero_or_positive_time_period_milliseconds,
        cv.Optional(CONF_BATTERY_SENSOR): _device_list(_battery_sensor.SCHEMA),
        cv.Optional(CONF_MANAGER): _device_list(_manager.SCHEMA),
        cv.Optional(CONF_REDVISION_DISPLAY): _device_list(_redvision_display.SCHEMA),
        cv.Optional(CONF_TVMS_ROGUE): _device_list(_tvms_rogue.SCHEMA),
        cv.Optional(CONF_TVMS_ROGUE_EMULATOR): _device_list(_tvms_rogue_emulator.SCHEMA),
        cv.Optional(CONF_TVMS_1280): _device_list(_tvms_1280.SCHEMA),
    }).extend(cv.COMPONENT_SCHEMA),
    cv.has_exactly_one_key(CONF_CANBUS, CONF_CANBUS_ID),
    _validate_unique_source_addresses,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    # Enable ESPHome's api client-connected trigger so the component can print the
    # discovered-device list when a logger connects (no on_client_connected: YAML
    # needed). The trigger and its firing code are compiled in only when defined.
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

    filter_interval = config[CONF_FILTER_INTERVAL]
    history_poll = config[CONF_HISTORY_POLL_INTERVAL]

    # Devices inherit the top-level filter_interval (all devices), host_address
    # (command-sending devices), and history_poll_interval (SOC/solar polling).
    # Rogue transition_length is intentionally configured only in tvms_rogue.
    def _inherit(device_config, host=False, soc_history=False, solar_history=False):
        device_config.setdefault(CONF_FILTER_INTERVAL, filter_interval)
        if host:
            device_config.setdefault(CONF_HOST_ADDRESS, host_address)
        if soc_history:
            device_config.setdefault(_battery_sensor.CONF_SOC_HISTORY_POLL_INTERVAL, history_poll)
        if solar_history:
            device_config.setdefault(_manager.CONF_SOLAR_HISTORY_POLL_INTERVAL, history_poll)
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
        await _tvms_rogue.to_code(_inherit(device_config, host=True))
    for device_config in config.get(CONF_TVMS_ROGUE_EMULATOR) or []:
        await _tvms_rogue_emulator.to_code(device_config)
    for device_config in config.get(CONF_TVMS_1280) or []:
        await _tvms_1280.to_code(_inherit(device_config, host=True))
