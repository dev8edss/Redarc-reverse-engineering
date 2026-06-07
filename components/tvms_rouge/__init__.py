import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor, button, light, number, sensor
from esphome.const import (
    CONF_ACCURACY_DECIMALS, CONF_DEFAULT_TRANSITION_LENGTH, CONF_DEVICE_CLASS,
    CONF_DISABLED_BY_DEFAULT, CONF_EFFECTS, CONF_ENTITY_CATEGORY,
    CONF_FLASH_TRANSITION_LENGTH, CONF_FORCE_UPDATE, CONF_GAMMA_CORRECT, CONF_ICON,
    CONF_ID, CONF_MODE, CONF_NAME, CONF_RESTORE_MODE, CONF_STATE_CLASS,
    CONF_UNIT_OF_MEASUREMENT,
    ENTITY_CATEGORY_CONFIG, ENTITY_CATEGORY_DIAGNOSTIC, ENTITY_CATEGORY_NONE,
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

# Resolve number mode AUTO via ESPHome's own mapping.
try:
    _NUMBER_MODE_AUTO = number.NUMBER_MODES.get("AUTO")
except AttributeError:
    _number_ns = cg.esphome_ns.namespace("number")
    _NUMBER_MODE_AUTO = _number_ns.enum("NumberMode", is_class=True).NUMBER_MODE_AUTO

CODEOWNERS = ["@dev8edss"]
AUTO_LOAD = ["binary_sensor", "button", "light", "number", "sensor", "redarc_common"]
MULTI_CONF = False

CONF_SOURCE_ADDRESS = "source_address"
CONF_HOST_ADDRESS = "host_address"
CONF_TRUE_OFF_THRESHOLD = "true_off_threshold"
CONF_TARGET_DEBOUNCE = "target_debounce"
CONF_START_DEADLINE = "start_deadline"
CONF_DEFAULT_DEADBAND = "default_deadband"
CONF_INITIAL_RATE = "initial_rate_ms_per_percent"
CONF_LEARNING_GAIN = "learning_gain"
CONF_APPROACH = "approach"
CONF_MAX_PULSE = "max_pulse"
CONF_SETTLE_TIME = "settle_time"
CONF_MAX_ITERATIONS = "max_iterations"

tvms_rouge_ns = cg.esphome_ns.namespace("tvms_rouge")
TVMSRougeComponent = tvms_rouge_ns.class_("TVMSRougeComponent", cg.Component)
TVMSRougeLight = tvms_rouge_ns.class_("TVMSRougeLight", light.LightOutput)
TVMSRougeNumber = tvms_rouge_ns.class_("TVMSRougeNumber", number.Number, cg.Component)
TVMSRougeButton = tvms_rouge_ns.class_("TVMSRougeButton", button.Button, cg.Component)

_sensor_ns = cg.esphome_ns.namespace("sensor")
_SensorClass = _sensor_ns.class_("Sensor")
_StateClass = _sensor_ns.enum("StateClass")
_SC_MAP = {
    "measurement": _StateClass.STATE_CLASS_MEASUREMENT,
    "total_increasing": _StateClass.STATE_CLASS_TOTAL_INCREASING,
}
_bs_ns = cg.esphome_ns.namespace("binary_sensor")
_BSClass = _bs_ns.class_("BinarySensor")

# Auto-generated IDs for sensors, lights, numbers, and the abort button
_AUTO_IDS = {}
for _i in range(1, 11):
    _AUTO_IDS[cv.GenerateID(f"light_out_{_i}")] = cv.declare_id(TVMSRougeLight)
    _AUTO_IDS[cv.GenerateID(f"light_state_{_i}")] = cv.declare_id(light.LightState)
    _AUTO_IDS[cv.GenerateID(f"level_sensor_{_i}")] = cv.declare_id(_SensorClass)
    _AUTO_IDS[cv.GenerateID(f"button_sensor_{_i}")] = cv.declare_id(_BSClass)
for _i in range(7):
    _AUTO_IDS[cv.GenerateID(f"num_{_i}")] = cv.declare_id(TVMSRougeNumber)
_AUTO_IDS[cv.GenerateID("tank1_id")] = cv.declare_id(_SensorClass)
_AUTO_IDS[cv.GenerateID("tank2_id")] = cv.declare_id(_SensorClass)
_AUTO_IDS[cv.GenerateID("input_voltage_id")] = cv.declare_id(_SensorClass)
_AUTO_IDS[cv.GenerateID("input_current_id")] = cv.declare_id(_SensorClass)
_AUTO_IDS[cv.GenerateID("abort_btn_id")] = cv.declare_id(TVMSRougeButton)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(TVMSRougeComponent),
        cv.Optional(CONF_SOURCE_ADDRESS, default=0x30): cv.hex_uint8_t,
        cv.Optional(CONF_HOST_ADDRESS, default=0x20): cv.hex_uint8_t,
        cv.Optional(CONF_TRUE_OFF_THRESHOLD, default=1.0): cv.float_range(min=0.0, max=10.0),
        cv.Optional(CONF_TARGET_DEBOUNCE, default="600ms"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_START_DEADLINE, default="2500ms"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_DEFAULT_DEADBAND, default=3.0): cv.float_range(min=0.1, max=20.0),
        cv.Optional(CONF_INITIAL_RATE, default=50.0): cv.float_range(min=10.0, max=250.0),
        cv.Optional(CONF_LEARNING_GAIN, default=0.25): cv.float_range(min=0.0, max=1.0),
        cv.Optional(CONF_APPROACH, default=0.80): cv.float_range(min=0.10, max=1.0),
        cv.Optional(CONF_MAX_PULSE, default="1200ms"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_SETTLE_TIME, default="700ms"): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_MAX_ITERATIONS, default=12): cv.int_range(min=1, max=50),
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
    cg.add(var.set_true_off_threshold(config[CONF_TRUE_OFF_THRESHOLD]))
    cg.add(var.set_target_debounce_ms(config[CONF_TARGET_DEBOUNCE].total_milliseconds))
    cg.add(var.set_start_deadline_ms(config[CONF_START_DEADLINE].total_milliseconds))
    cg.add(var.set_deadband_percent(config[CONF_DEFAULT_DEADBAND]))
    cg.add(var.set_initial_rate_ms_per_percent(config[CONF_INITIAL_RATE]))
    cg.add(var.set_learning_gain(config[CONF_LEARNING_GAIN]))
    cg.add(var.set_approach(config[CONF_APPROACH]))
    cg.add(var.set_max_pulse_ms(config[CONF_MAX_PULSE].total_milliseconds))
    cg.add(var.set_settle_ms(config[CONF_SETTLE_TIME].total_milliseconds))
    cg.add(var.set_max_iterations(config[CONF_MAX_ITERATIONS]))

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

    # Output level sensors and button state binary sensors
    for i in range(1, 11):
        ls = await _make_sensor(config[f"level_sensor_{i}"], f"{p} Output {i} Level",
                                unit="%", state_class=STATE_CLASS_MEASUREMENT, decimals=0,
                                entity_category=ENTITY_CATEGORY_DIAGNOSTIC)
        cg.add(var.set_level_sensor(i, ls))

        bs = cg.new_Pvariable(config[f"button_sensor_{i}"])
        bs_cfg = {
            CONF_ID: config[f"button_sensor_{i}"],
            CONF_NAME: f"{p} Output {i} Button Active",
            CONF_DISABLED_BY_DEFAULT: False,
            CONF_ICON: "",
            CONF_ENTITY_CATEGORY: ENTITY_CATEGORY_DIAGNOSTIC,
        }
        await binary_sensor.register_binary_sensor(bs, bs_cfg)
        cg.add(var.set_button_sensor(i, bs))

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
            CONF_DEFAULT_TRANSITION_LENGTH: 0,
            CONF_FLASH_TRANSITION_LENGTH: 250,
            CONF_DISABLED_BY_DEFAULT: False,
            CONF_RESTORE_MODE: _RESTORE_MODE_OFF,
            CONF_EFFECTS: [],
        }
        await light.register_light(light_out, light_cfg)

    # Tuning number entities
    TUNING = [
        ("Deadband", 0, 0.5, 10.0, 0.5, 3.0, "%"),
        ("Initial Ramp Rate", 1, 20.0, 160.0, 1.0, 50.0, "ms/%"),
        ("Learning Gain", 2, 1.0, 100.0, 1.0, 25.0, "%"),
        ("Approach", 3, 25.0, 100.0, 5.0, 80.0, "%"),
        ("Max Pulse", 4, 200.0, 5000.0, 50.0, 1200.0, "ms"),
        ("Settle Time", 5, 200.0, 3000.0, 50.0, 700.0, "ms"),
        ("Max Iterations", 6, 1.0, 30.0, 1.0, 12.0, ""),
    ]
    for suffix, param_idx, min_v, max_v, step, initial, unit in TUNING:
        num = cg.new_Pvariable(config[f"num_{param_idx}"])
        cg.add(num.set_parent(var))
        cg.add(num.set_parameter(param_idx))
        cg.add(num.set_initial_value(initial))
        num_cfg = {
            CONF_ID: config[f"num_{param_idx}"],
            CONF_NAME: f"{p} {suffix}",
            CONF_DISABLED_BY_DEFAULT: False,
            CONF_ICON: "",
            CONF_ENTITY_CATEGORY: ENTITY_CATEGORY_CONFIG,
            CONF_MODE: _NUMBER_MODE_AUTO,
        }
        if unit:
            num_cfg[CONF_UNIT_OF_MEASUREMENT] = unit
        await number.register_number(num, num_cfg, min_value=min_v, max_value=max_v, step=step)

    # Abort button
    btn = cg.new_Pvariable(config["abort_btn_id"])
    cg.add(btn.set_parent(var))
    btn_cfg = {
        CONF_ID: config["abort_btn_id"],
        CONF_NAME: f"{p} Abort and Release",
        CONF_DISABLED_BY_DEFAULT: False,
        CONF_ICON: "",
        CONF_ENTITY_CATEGORY: ENTITY_CATEGORY_DIAGNOSTIC,
    }
    await button.register_button(btn, btn_cfg)
