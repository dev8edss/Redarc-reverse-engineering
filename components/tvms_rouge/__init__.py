import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor, button, canbus, light, number, sensor
from esphome.const import (
    CONF_DEFAULT_TRANSITION_LENGTH,
    CONF_GAMMA_CORRECT,
    CONF_ID,
    CONF_NAME,
)

CODEOWNERS = ["@dev8edss"]
DEPENDENCIES = ["canbus"]
AUTO_LOAD = ["binary_sensor", "button", "light", "number", "sensor"]
MULTI_CONF = False

CONF_CANBUS_ID = "canbus_id"
CONF_OUTPUT_COMMAND_ID = "output_command_id"
CONF_DIM_COMMAND_ID = "dim_command_id"
CONF_KEEPALIVE_ID = "keepalive_id"
CONF_LEVEL_FEEDBACK_ID = "level_feedback_id"
CONF_TANK_FEEDBACK_ID = "tank_feedback_id"
CONF_BUTTON_STATUS_ID = "button_status_id"
CONF_INPUT_STATUS_ID = "input_status_id"
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
        cv.Required(CONF_CANBUS_ID): cv.use_id(canbus.CanbusComponent),
        cv.Optional(CONF_OUTPUT_COMMAND_ID, default=0x0F003020): cv.hex_uint32_t,
        cv.Optional(CONF_DIM_COMMAND_ID, default=0x0F053020): cv.hex_uint32_t,
        cv.Optional(CONF_KEEPALIVE_ID, default=0x0FE6FF20): cv.hex_uint32_t,
        cv.Optional(CONF_LEVEL_FEEDBACK_ID, default=0x1BFD1230): cv.hex_uint32_t,
        cv.Optional(CONF_TANK_FEEDBACK_ID, default=0x1BFD0230): cv.hex_uint32_t,
        cv.Optional(CONF_BUTTON_STATUS_ID, default=0x1BFD1430): cv.hex_uint32_t,
        cv.Optional(CONF_INPUT_STATUS_ID, default=0x13F10830): cv.hex_uint32_t,
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


def _make_sensor(config_id, name, unit=None, device_class=None, state_class_raw=None,
                 decimals=None, entity_category_raw=None):
    var = cg.new_Pvariable(config_id)
    cg.add(var.set_name(name))
    if unit:
        cg.add(var.set_unit_of_measurement(unit))
    if device_class:
        cg.add(var.set_device_class(device_class))
    if state_class_raw:
        cg.add(var.set_state_class(cg.RawExpression(state_class_raw)))
    if decimals is not None:
        cg.add(var.set_accuracy_decimals(decimals))
    if entity_category_raw:
        cg.add(var.set_entity_category(cg.RawExpression(entity_category_raw)))
    cg.add(cg.App.register_component(var))
    cg.add(cg.App.register_sensor(var))
    return var


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    can = await cg.get_variable(config[CONF_CANBUS_ID])
    cg.add(var.set_canbus(can))
    cg.add(var.set_output_command_id(config[CONF_OUTPUT_COMMAND_ID]))
    cg.add(var.set_dim_command_id(config[CONF_DIM_COMMAND_ID]))
    cg.add(var.set_keepalive_id(config[CONF_KEEPALIVE_ID]))
    cg.add(var.set_level_feedback_id(config[CONF_LEVEL_FEEDBACK_ID]))
    cg.add(var.set_tank_feedback_id(config[CONF_TANK_FEEDBACK_ID]))
    cg.add(var.set_button_status_id(config[CONF_BUTTON_STATUS_ID]))
    cg.add(var.set_input_status_id(config[CONF_INPUT_STATUS_ID]))
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
    SC = "sensor::StateClass::STATE_CLASS_MEASUREMENT"
    DIAG = "ENTITY_CATEGORY_DIAGNOSTIC"
    CFG = "ENTITY_CATEGORY_CONFIG"

    # Sensors
    s = _make_sensor(config["tank1_id"], f"{p} Tank 1",
                     unit="%", state_class_raw=SC, decimals=0)
    cg.add(var.set_tank1_sensor(s))

    s = _make_sensor(config["tank2_id"], f"{p} Tank 2",
                     unit="%", state_class_raw=SC, decimals=0)
    cg.add(var.set_tank2_sensor(s))

    s = _make_sensor(config["input_voltage_id"], f"{p} Input Voltage",
                     unit="V", device_class="voltage", state_class_raw=SC, decimals=2)
    cg.add(var.set_input_voltage_sensor(s))

    s = _make_sensor(config["input_current_id"], f"{p} Input Current",
                     unit="A", device_class="current", state_class_raw=SC, decimals=1,
                     entity_category_raw=DIAG)
    cg.add(var.set_input_current_sensor(s))

    # Output level sensors and button state binary sensors
    for i in range(1, 11):
        ls = _make_sensor(config[f"level_sensor_{i}"], f"{p} Output {i} Level",
                          unit="%", state_class_raw=SC, decimals=0,
                          entity_category_raw=DIAG)
        cg.add(var.set_level_sensor(i, ls))

        bs = cg.new_Pvariable(config[f"button_sensor_{i}"])
        cg.add(bs.set_name(f"{p} Output {i} Button Active"))
        cg.add(cg.App.register_component(bs))
        cg.add(cg.App.register_binary_sensor(bs))
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
        cg.add(num.set_name(f"{p} {suffix}"))
        cg.add(num.set_entity_category(cg.RawExpression(CFG)))
        if unit:
            cg.add(num.set_unit_of_measurement(unit))
        cg.add(num.traits.set_min_value(min_v))
        cg.add(num.traits.set_max_value(max_v))
        cg.add(num.traits.set_step(step))
        cg.add(num.set_parent(var))
        cg.add(num.set_parameter(param_idx))
        cg.add(num.set_initial_value(initial))
        cg.add(cg.App.register_component(num))
        cg.add(cg.App.register_number(num))

    # Abort button
    btn = cg.new_Pvariable(config["abort_btn_id"])
    cg.add(btn.set_name(f"{p} Abort and Release"))
    cg.add(btn.set_entity_category(cg.RawExpression(DIAG)))
    cg.add(btn.set_parent(var))
    cg.add(cg.App.register_component(btn))
    cg.add(cg.App.register_button(btn))
