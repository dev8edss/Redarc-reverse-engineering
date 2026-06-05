import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import CONF_ID

CODEOWNERS = ["@dev8edss"]
AUTO_LOAD = ["sensor", "redarc_common"]
MULTI_CONF = False

CONF_SOURCE_ADDRESS = "source_address"
CONF_CURRENT = "current"
CONF_CURRENT_RAW = "current_raw"
CONF_VOLTAGE = "voltage"
CONF_TEMPERATURE = "temperature"
CONF_SOC = "soc"

battery_sensor_ns = cg.esphome_ns.namespace("battery_sensor")
BatterySensorComponent = battery_sensor_ns.class_("BatterySensorComponent", cg.Component)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(BatterySensorComponent),
    cv.Optional(CONF_SOURCE_ADDRESS, default=0x08): cv.hex_uint8_t,
    cv.Optional(CONF_CURRENT): sensor.sensor_schema(),
    cv.Optional(CONF_CURRENT_RAW): sensor.sensor_schema(),
    cv.Optional(CONF_VOLTAGE): sensor.sensor_schema(),
    cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(),
    cv.Optional(CONF_SOC): sensor.sensor_schema(),
}).extend(cv.COMPONENT_SCHEMA)

async def _setup_sensor(var, config, key, setter):
    if key in config:
        sens = await sensor.new_sensor(config[key])
        cg.add(getattr(var, setter)(sens))

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_source_address(config[CONF_SOURCE_ADDRESS]))
    await _setup_sensor(var, config, CONF_CURRENT, "set_current_sensor")
    await _setup_sensor(var, config, CONF_CURRENT_RAW, "set_current_raw_sensor")
    await _setup_sensor(var, config, CONF_VOLTAGE, "set_voltage_sensor")
    await _setup_sensor(var, config, CONF_TEMPERATURE, "set_temperature_sensor")
    await _setup_sensor(var, config, CONF_SOC, "set_soc_sensor")
