import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import CONF_ID

CODEOWNERS = ["@dev8edss"]
AUTO_LOAD = ["sensor", "redarc_common"]
MULTI_CONF = True

CONF_SOURCE_ADDRESS = "source_address"
CONF_DISPLAY_TYPE = "display_type"
CONF_BATTERY_CURRENT_DISPLAY = "battery_current_display"
CONF_DEVICE_CURRENT_DISPLAY = "device_current_display"
CONF_MANAGER_OUTPUT_CURRENT_DISPLAY = "manager_output_current_display"
CONF_BATTERY_CURRENT_DISPLAY_RAW = "battery_current_display_raw"
CONF_DEVICE_CURRENT_DISPLAY_RAW = "device_current_display_raw"
CONF_MANAGER_OUTPUT_CURRENT_DISPLAY_RAW = "manager_output_current_display_raw"

ns = cg.esphome_ns.namespace("redvision_display")
RedvisionDisplayComponent = ns.class_("RedvisionDisplayComponent", cg.Component)

DISPLAY_TYPE = cv.enum({"rv1": 1, "rv2": 2}, lower=True)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(RedvisionDisplayComponent),
    cv.Required(CONF_SOURCE_ADDRESS): cv.hex_uint8_t,
    cv.Required(CONF_DISPLAY_TYPE): DISPLAY_TYPE,
    cv.Optional(CONF_BATTERY_CURRENT_DISPLAY): sensor.sensor_schema(),
    cv.Optional(CONF_DEVICE_CURRENT_DISPLAY): sensor.sensor_schema(),
    cv.Optional(CONF_MANAGER_OUTPUT_CURRENT_DISPLAY): sensor.sensor_schema(),
    cv.Optional(CONF_BATTERY_CURRENT_DISPLAY_RAW): sensor.sensor_schema(),
    cv.Optional(CONF_DEVICE_CURRENT_DISPLAY_RAW): sensor.sensor_schema(),
    cv.Optional(CONF_MANAGER_OUTPUT_CURRENT_DISPLAY_RAW): sensor.sensor_schema(),
}).extend(cv.COMPONENT_SCHEMA)

async def _setup_sensor(var, config, key, setter):
    if key in config:
        sens = await sensor.new_sensor(config[key])
        cg.add(getattr(var, setter)(sens))

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_source_address(config[CONF_SOURCE_ADDRESS]))
    cg.add(var.set_display_type(config[CONF_DISPLAY_TYPE]))
    await _setup_sensor(var, config, CONF_BATTERY_CURRENT_DISPLAY, "set_battery_current_display_sensor")
    await _setup_sensor(var, config, CONF_DEVICE_CURRENT_DISPLAY, "set_device_current_display_sensor")
    await _setup_sensor(var, config, CONF_MANAGER_OUTPUT_CURRENT_DISPLAY, "set_manager_output_current_display_sensor")
    await _setup_sensor(var, config, CONF_BATTERY_CURRENT_DISPLAY_RAW, "set_battery_current_display_raw_sensor")
    await _setup_sensor(var, config, CONF_DEVICE_CURRENT_DISPLAY_RAW, "set_device_current_display_raw_sensor")
    await _setup_sensor(var, config, CONF_MANAGER_OUTPUT_CURRENT_DISPLAY_RAW, "set_manager_output_current_display_raw_sensor")
