import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor, light, sensor, switch
from esphome.const import (
    CONF_ACCURACY_DECIMALS, CONF_DEFAULT_TRANSITION_LENGTH, CONF_DEVICE_CLASS,
    CONF_DISABLED_BY_DEFAULT, CONF_EFFECTS, CONF_ENTITY_CATEGORY,
    CONF_FLASH_TRANSITION_LENGTH, CONF_FORCE_UPDATE, CONF_GAMMA_CORRECT, CONF_ICON,
    CONF_ID, CONF_NAME, CONF_RESTORE_MODE, CONF_STATE_CLASS,
    CONF_UNIT_OF_MEASUREMENT,
    ENTITY_CATEGORY_DIAGNOSTIC, ENTITY_CATEGORY_NONE,
    STATE_CLASS_MEASUREMENT,
)

# Resolve restore mode via ESPHome's own mapping so the correct C++ enum name
# is used regardless of ESPHome version (RESTORE_DEFAULT_OFF removed in 2026.5.x).
try:
    _RESTORE_MODE_OFF = (
        light.RESTORE_MODES.get("RESTORE_DEFAULT_OFF")
        or light.RESTORE_MODES.get("RESTORE_AND_OFF")
        or next(iter(light.RESTORE_MODES.values()))
    )
except AttributeError:
    _light_ns = cg.esphome_ns.namespace("light")
    _RESTORE_MODE_OFF = _light_ns.enum("LightRestoreMode", is_class=True).RESTORE_AND_OFF

try:
    _SWITCH_RESTORE_MODE_OFF = (
        switch.RESTORE_MODES.get("RESTORE_DEFAULT_OFF")
        or switch.RESTORE_MODES.get("RESTORE_AND_OFF")
        or switch.RESTORE_MODES.get("ALWAYS_OFF")
        or next(iter(switch.RESTORE_MODES.values()))
    )
except AttributeError:
    _switch_ns = cg.esphome_ns.namespace("switch_")
    _SWITCH_RESTORE_MODE_OFF = _switch_ns.enum("SwitchRestoreMode", is_class=True).SWITCH_RESTORE_DEFAULT_OFF

CODEOWNERS = ["@dev8edss"]
AUTO_LOAD = ["binary_sensor", "light", "sensor", "switch", "redarc_common"]
MULTI_CONF = False

CONF_SOURCE_ADDRESS = "source_address"
CONF_HOST_ADDRESS = "host_address"
CONF_FILTER_INTERVAL = "filter_interval"
CONF_TRUE_OFF_THRESHOLD = "true_off_threshold"

tvms_rouge_ns = cg.esphome_ns.namespace("redarc_tvms_rouge")
TVMSRougeComponent = tvms_rouge_ns.class_("TVMSRougeComponent", cg.Component)
TVMSRougeLight = tvms_rouge_ns.class_("TVMSRougeLight", light.LightOutput)
TVMSRougeSwitch = tvms_rouge_ns.class_("TVMSRougeSwitch", switch.Switch)

_sensor_ns = cg.esphome_ns.namespace("sensor")
_SensorClass = _sensor_ns.class_("Sensor")
_StateClass = _sensor_ns.enum("StateClass")
_SC_MAP = {
    "measurement": _StateClass.STATE_CLASS_MEASUREMENT,
    "total_increasing": _StateClass.STATE_CLASS_TOTAL_INCREASING,
}
_bs_ns = cg.esphome_ns.namespace("binary_sensor")
_BSClass = _bs_ns.class_("BinarySensor")

# Auto-generated IDs for sensors and lights.
_AUTO_IDS = {}
for _i in range(1, 11):
    _AUTO_IDS[cv.GenerateID(f"light_out_{_i}")] = cv.declare_id(TVMSRougeLight)
    _AUTO_IDS[cv.GenerateID(f"light_state_{_i}")] = cv.declare_id(light.LightState)
    _AUTO_IDS[cv.GenerateID(f"level_sensor_{_i}")] = cv.declare_id(_SensorClass)
for _i in range(1, 9):
    _AUTO_IDS[cv.GenerateID(f"button_sensor_{_i}")] = cv.declare_id(_BSClass)
_AUTO_IDS[cv.GenerateID("tank1_id")] = cv.declare_id(_SensorClass)
_AUTO_IDS[cv.GenerateID("tank2_id")] = cv.declare_id(_SensorClass)
_AUTO_IDS[cv.GenerateID("input_voltage_id")] = cv.declare_id(_SensorClass)
_AUTO_IDS[cv.GenerateID("input_current_id")] = cv.declare_id(_SensorClass)
_AUTO_IDS[cv.GenerateID("master_id")] = cv.declare_id(TVMSRougeSwitch)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(TVMSRougeComponent),
        cv.Optional(CONF_SOURCE_ADDRESS, default=0x30): cv.hex_uint8_t,
        cv.Optional(CONF_HOST_ADDRESS, default=0x20): cv.hex_uint8_t,
        cv.Optional(CONF_FILTER_INTERVAL, default="5s"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_TRUE_OFF_THRESHOLD, default=1.5): cv.float_range(min=0.0, max=10.0),
        cv.Optional(CONF_DEFAULT_TRANSITION_LENGTH, default="0s"): cv.positive_time_period_milliseconds,
        **_AUTO_IDS,
    }
).extend(cv.COMPONENT_SCHEMA)


async def _make_sensor(config_id, name, unit=None, device_class=None,
                       state_class=None, decimals=None, entity_category=None):
    cfg = {
        CONF_ID: config_id,
        CONF_NAME: name,
        CONF_DISABLED_BY_DEFAULT: False,
        CONF_FORCE_UPDATE: False,
        CONF_ICON: "",
        CONF_ENTITY_CATEGORY: entity_category if entity_category is not None else ENTITY_CATEGORY_NONE,
    }
    if unit is not None:
        cfg[CONF_UNIT_OF_MEASUREMENT] = unit
    if device_class is not None:
        cfg[CONF_DEVICE_CLASS] = device_class
    if state_class is not None:
        cfg[CONF_STATE_CLASS] = _SC_MAP.get(state_class, state_class)
    if decimals is not None:
        cfg[CONF_ACCURACY_DECIMALS] = decimals
    return await sensor.new_sensor(cfg)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_source_address(config[CONF_SOURCE_ADDRESS]))
    cg.add(var.set_host_address(config[CONF_HOST_ADDRESS]))
    cg.add(var.set_filter_interval_ms(config[CONF_FILTER_INTERVAL]))
    cg.add(var.set_true_off_threshold(config[CONF_TRUE_OFF_THRESHOLD]))
    cg.add(var.set_default_transition_length_ms(config[CONF_DEFAULT_TRANSITION_LENGTH]))

    p = config[CONF_ID].id.replace("_", " ")

    # Sensors
    s = await _make_sensor(config["tank1_id"], f"{p} Tank 1",
                           unit="%", state_class=STATE_CLASS_MEASUREMENT, decimals=0)
    cg.add(var.set_tank1_sensor(s))

    s = await _make_sensor(config["tank2_id"], f"{p} Tank 2",
                           unit="%", state_class=STATE_CLASS_MEASUREMENT, decimals=0)
    cg.add(var.set_tank2_sensor(s))

    s = await _make_sensor(config["input_voltage_id"], f"{p} Input Voltage",
                           unit="V", device_class="voltage",
                           state_class=STATE_CLASS_MEASUREMENT, decimals=2)
    cg.add(var.set_input_voltage_sensor(s))

    s = await _make_sensor(config["input_current_id"], f"{p} Input Current",
                           unit="A", device_class="current",
                           state_class=STATE_CLASS_MEASUREMENT, decimals=1,
                           entity_category=ENTITY_CATEGORY_DIAGNOSTIC)
    cg.add(var.set_input_current_sensor(s))

    # Output level sensors
    for i in range(1, 11):
        ls = await _make_sensor(config[f"level_sensor_{i}"], f"{p} Output {i} Level",
                                unit="%", state_class=STATE_CLASS_MEASUREMENT, decimals=0,
                                entity_category=ENTITY_CATEGORY_DIAGNOSTIC)
        cg.add(var.set_level_sensor(i, ls))

    # Physical input/button state binary sensors
    for i in range(1, 9):
        bs = cg.new_Pvariable(config[f"button_sensor_{i}"])
        bs_cfg = {
            CONF_ID: config[f"button_sensor_{i}"],
            CONF_NAME: f"{p} Input Button {i}",
            CONF_DISABLED_BY_DEFAULT: False,
            CONF_ICON: "",
            CONF_ENTITY_CATEGORY: ENTITY_CATEGORY_DIAGNOSTIC,
        }
        await binary_sensor.register_binary_sensor(bs, bs_cfg)
        cg.add(var.set_button_sensor(i, bs))

    # Master switch (channel 0x0B)
    master = cg.new_Pvariable(config["master_id"])
    cg.add(master.set_parent(var))
    master_cfg = {
        CONF_ID: config["master_id"],
        CONF_NAME: f"{p} Master",
        CONF_DISABLED_BY_DEFAULT: False,
        CONF_ICON: "",
        CONF_ENTITY_CATEGORY: ENTITY_CATEGORY_NONE,
        CONF_RESTORE_MODE: _SWITCH_RESTORE_MODE_OFF,
    }
    await switch.register_switch(master, master_cfg)
    cg.add(var.register_master_switch(master))

    # Lights
    for i in range(1, 11):
        channel = 0x0B + i  # 0x0C .. 0x15
        light_out = cg.new_Pvariable(config[f"light_out_{i}"])
        cg.add(light_out.set_parent(var))
        cg.add(light_out.set_output_number(i))
        cg.add(light_out.set_channel(channel))
        cg.add(var.register_light(light_out))
        light_cfg = {
            CONF_ID: config[f"light_state_{i}"],
            CONF_NAME: f"{p} Output {i}",
            CONF_GAMMA_CORRECT: 1.0,
            CONF_DEFAULT_TRANSITION_LENGTH: config[CONF_DEFAULT_TRANSITION_LENGTH],
            CONF_FLASH_TRANSITION_LENGTH: 250,
            CONF_DISABLED_BY_DEFAULT: False,
            CONF_RESTORE_MODE: _RESTORE_MODE_OFF,
            CONF_EFFECTS: [],
        }
        await light.register_light(light_out, light_cfg)
