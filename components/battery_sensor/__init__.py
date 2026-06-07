import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import CONF_ID

CODEOWNERS = ["@dev8edss"]
AUTO_LOAD = ["sensor", "redarc_common"]
MULTI_CONF = False

CONF_SOURCE_ADDRESS = "source_address"
CONF_FILTER_INTERVAL = "filter_interval"

battery_sensor_ns = cg.esphome_ns.namespace("battery_sensor")
BatterySensorComponent = battery_sensor_ns.class_("BatterySensorComponent", cg.Component)

_sensor_ns = cg.esphome_ns.namespace("sensor")
_SensorClass = _sensor_ns.class_("Sensor")

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(BatterySensorComponent),
    cv.Optional(CONF_SOURCE_ADDRESS, default=0x08): cv.hex_uint8_t,
    cv.Optional(CONF_FILTER_INTERVAL, default="5s"): cv.positive_time_period_milliseconds,
    cv.GenerateID("current_id"): cv.declare_id(_SensorClass),
    cv.GenerateID("current_raw_id"): cv.declare_id(_SensorClass),
    cv.GenerateID("voltage_id"): cv.declare_id(_SensorClass),
    cv.GenerateID("temperature_id"): cv.declare_id(_SensorClass),
    cv.GenerateID("soc_id"): cv.declare_id(_SensorClass),
}).extend(cv.COMPONENT_SCHEMA)


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
    cg.add(var.set_source_address(config[CONF_SOURCE_ADDRESS]))
    cg.add(var.set_filter_interval_ms(config[CONF_FILTER_INTERVAL].total_milliseconds))

    p = config[CONF_ID].id.replace("_", " ")

    s = _make_sensor(config["current_id"], f"{p} Current",
                     unit="A", device_class="current",
                     state_class_raw="sensor::StateClass::STATE_CLASS_MEASUREMENT", decimals=3)
    cg.add(var.set_current_sensor(s))

    s = _make_sensor(config["current_raw_id"], f"{p} Current Raw",
                     state_class_raw="sensor::StateClass::STATE_CLASS_MEASUREMENT", decimals=0,
                     entity_category_raw="ENTITY_CATEGORY_DIAGNOSTIC")
    cg.add(var.set_current_raw_sensor(s))

    s = _make_sensor(config["voltage_id"], f"{p} Voltage",
                     unit="V", device_class="voltage",
                     state_class_raw="sensor::StateClass::STATE_CLASS_MEASUREMENT", decimals=3)
    cg.add(var.set_voltage_sensor(s))

    s = _make_sensor(config["temperature_id"], f"{p} Temperature",
                     unit="°C", device_class="temperature",
                     state_class_raw="sensor::StateClass::STATE_CLASS_MEASUREMENT", decimals=0)
    cg.add(var.set_temperature_sensor(s))

    s = _make_sensor(config["soc_id"], f"{p} SOC",
                     unit="%", device_class="battery",
                     state_class_raw="sensor::StateClass::STATE_CLASS_MEASUREMENT", decimals=0)
    cg.add(var.set_soc_sensor(s))
