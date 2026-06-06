import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import CONF_ID
from esphome.components.battery_sensor import BatterySensorComponent

CODEOWNERS = ["@dev8edss"]
AUTO_LOAD = ["sensor", "redarc_common", "battery_sensor"]
MULTI_CONF = False

CONF_SOURCE_ADDRESS = "source_address"
CONF_BATTERY_SENSOR_ID = "battery_sensor_id"
CONF_OUTPUT_CURRENT = "output_current"
CONF_OUTPUT_CURRENT_RAW = "output_current_raw"
CONF_BATTERY_VOLTAGE = "battery_voltage"
CONF_SOURCE_DEVICE_CURRENT = "source_device_current"
CONF_SOLAR_CURRENT = "solar_current"
CONF_SOLAR_VOLTAGE = "solar_voltage"
CONF_SOLAR_POWER = "solar_power"
CONF_SOLAR_ENERGY = "solar_energy"
CONF_AC_INPUT_VOLTAGE = "ac_input_voltage"

manager30_ns = cg.esphome_ns.namespace("manager30")
Manager30Component = manager30_ns.class_("Manager30Component", cg.Component)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(Manager30Component),
    cv.Optional(CONF_SOURCE_ADDRESS, default=0x01): cv.hex_uint8_t,
    cv.Optional(CONF_BATTERY_SENSOR_ID): cv.use_id(BatterySensorComponent),
    cv.Optional(CONF_OUTPUT_CURRENT): sensor.sensor_schema(),
    cv.Optional(CONF_OUTPUT_CURRENT_RAW): sensor.sensor_schema(),
    cv.Optional(CONF_BATTERY_VOLTAGE): sensor.sensor_schema(),
    cv.Optional(CONF_SOURCE_DEVICE_CURRENT): sensor.sensor_schema(),
    cv.Optional(CONF_SOLAR_CURRENT): sensor.sensor_schema(),
    cv.Optional(CONF_SOLAR_VOLTAGE): sensor.sensor_schema(),
    cv.Optional(CONF_SOLAR_POWER): sensor.sensor_schema(),
    cv.Optional(CONF_SOLAR_ENERGY): sensor.sensor_schema(),
    cv.Optional(CONF_AC_INPUT_VOLTAGE): sensor.sensor_schema(),
}).extend(cv.COMPONENT_SCHEMA)

async def _setup_sensor(var, config, key, setter):
    if key in config:
        sens = await sensor.new_sensor(config[key])
        cg.add(getattr(var, setter)(sens))

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_source_address(config[CONF_SOURCE_ADDRESS]))
    if CONF_BATTERY_SENSOR_ID in config:
        batt = await cg.get_variable(config[CONF_BATTERY_SENSOR_ID])
        cg.add(var.set_battery_sensor(batt))
    await _setup_sensor(var, config, CONF_OUTPUT_CURRENT, "set_output_current_sensor")
    await _setup_sensor(var, config, CONF_OUTPUT_CURRENT_RAW, "set_output_current_raw_sensor")
    await _setup_sensor(var, config, CONF_BATTERY_VOLTAGE, "set_battery_voltage_sensor")
    await _setup_sensor(var, config, CONF_SOURCE_DEVICE_CURRENT, "set_source_device_current_sensor")
    await _setup_sensor(var, config, CONF_SOLAR_CURRENT, "set_solar_current_sensor")
    await _setup_sensor(var, config, CONF_SOLAR_VOLTAGE, "set_solar_voltage_sensor")
    await _setup_sensor(var, config, CONF_SOLAR_POWER, "set_solar_power_sensor")
    await _setup_sensor(var, config, CONF_SOLAR_ENERGY, "set_solar_energy_sensor")
    await _setup_sensor(var, config, CONF_AC_INPUT_VOLTAGE, "set_ac_input_voltage_sensor")
