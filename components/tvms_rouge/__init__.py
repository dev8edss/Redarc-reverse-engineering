import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import canbus, sensor
from esphome.const import CONF_ID

CODEOWNERS = ["@dev8edss"]
DEPENDENCIES = ["canbus"]
AUTO_LOAD = ["light", "sensor"]
MULTI_CONF = False

CONF_CANBUS_ID = "canbus_id"
CONF_OUTPUT_COMMAND_ID = "output_command_id"
CONF_DIM_COMMAND_ID = "dim_command_id"
CONF_KEEPALIVE_ID = "keepalive_id"
CONF_LEVEL_FEEDBACK_ID = "level_feedback_id"
CONF_TANK_FEEDBACK_ID = "tank_feedback_id"
CONF_TANK1 = "tank1"
CONF_TANK2 = "tank2"
CONF_OUTPUT_LEVELS = "output_levels"
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

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(TVMSRougeComponent),
        cv.Required(CONF_CANBUS_ID): cv.use_id(canbus.Canbus),
        cv.Optional(CONF_OUTPUT_COMMAND_ID, default=0x0F003020): cv.hex_uint32_t,
        cv.Optional(CONF_DIM_COMMAND_ID, default=0x0F053020): cv.hex_uint32_t,
        cv.Optional(CONF_KEEPALIVE_ID, default=0x0FE6FF20): cv.hex_uint32_t,
        cv.Optional(CONF_LEVEL_FEEDBACK_ID, default=0x1BFD1230): cv.hex_uint32_t,
        cv.Optional(CONF_TANK_FEEDBACK_ID, default=0x1BFD0230): cv.hex_uint32_t,
        cv.Optional(CONF_TANK1): sensor.sensor_schema(),
        cv.Optional(CONF_TANK2): sensor.sensor_schema(),
        cv.Optional(CONF_OUTPUT_LEVELS): cv.ensure_list(sensor.sensor_schema()),
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
    }
).extend(cv.COMPONENT_SCHEMA)

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

    if CONF_TANK1 in config:
        sens = await sensor.new_sensor(config[CONF_TANK1])
        cg.add(var.set_tank1_sensor(sens))
    if CONF_TANK2 in config:
        sens = await sensor.new_sensor(config[CONF_TANK2])
        cg.add(var.set_tank2_sensor(sens))
    if CONF_OUTPUT_LEVELS in config:
        for idx, level_conf in enumerate(config[CONF_OUTPUT_LEVELS], start=1):
            if idx > 10:
                break
            sens = await sensor.new_sensor(level_conf)
            cg.add(var.set_level_sensor(idx, sens))
