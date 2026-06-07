import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_ACCURACY_DECIMALS, CONF_DEVICE_CLASS, CONF_DISABLED_BY_DEFAULT,
    CONF_ENTITY_CATEGORY, CONF_FORCE_UPDATE, CONF_ICON, CONF_ID, CONF_NAME,
    CONF_STATE_CLASS, CONF_UNIT_OF_MEASUREMENT,
    ENTITY_CATEGORY_DIAGNOSTIC, ENTITY_CATEGORY_NONE,
    STATE_CLASS_MEASUREMENT, STATE_CLASS_TOTAL_INCREASING,
)

CODEOWNERS = ["@dev8edss"]
AUTO_LOAD = ["sensor", "redarc_common"]
MULTI_CONF = False

CONF_SOURCE_ADDRESS = "source_address"
CONF_FILTER_INTERVAL = "filter_interval"

manager30_ns = cg.esphome_ns.namespace("manager30")
Manager30Component = manager30_ns.class_("Manager30Component", cg.Component)

_sensor_ns = cg.esphome_ns.namespace("sensor")
_SensorClass = _sensor_ns.class_("Sensor")

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(Manager30Component),
    cv.Optional(CONF_SOURCE_ADDRESS, default=0x01): cv.hex_uint8_t,
    cv.Optional(CONF_FILTER_INTERVAL, default="5s"): cv.positive_time_period_milliseconds,
    cv.GenerateID("output_current_id"): cv.declare_id(_SensorClass),
    cv.GenerateID("output_current_raw_id"): cv.declare_id(_SensorClass),
    cv.GenerateID("battery_voltage_id"): cv.declare_id(_SensorClass),
    cv.GenerateID("solar_current_id"): cv.declare_id(_SensorClass),
    cv.GenerateID("solar_voltage_id"): cv.declare_id(_SensorClass),
    cv.GenerateID("solar_power_id"): cv.declare_id(_SensorClass),
    cv.GenerateID("solar_energy_id"): cv.declare_id(_SensorClass),
    cv.GenerateID("ac_input_voltage_id"): cv.declare_id(_SensorClass),
}).extend(cv.COMPONENT_SCHEMA)


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
        cfg[CONF_STATE_CLASS] = state_class
    if decimals is not None:
        cfg[CONF_ACCURACY_DECIMALS] = decimals
    return await sensor.new_sensor(cfg)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_source_address(config[CONF_SOURCE_ADDRESS]))

    p = config[CONF_ID].id.replace("_", " ")

    s = await _make_sensor(config["output_current_id"], f"{p} Output Current",
                           unit="A", device_class="current",
                           state_class=STATE_CLASS_MEASUREMENT, decimals=3)
    cg.add(var.set_output_current_sensor(s))

    s = await _make_sensor(config["output_current_raw_id"], f"{p} Output Current Raw",
                           state_class=STATE_CLASS_MEASUREMENT, decimals=0,
                           entity_category=ENTITY_CATEGORY_DIAGNOSTIC)
    cg.add(var.set_output_current_raw_sensor(s))

    s = await _make_sensor(config["battery_voltage_id"], f"{p} Battery Voltage",
                           unit="V", device_class="voltage",
                           state_class=STATE_CLASS_MEASUREMENT, decimals=3)
    cg.add(var.set_battery_voltage_sensor(s))

    s = await _make_sensor(config["solar_current_id"], f"{p} Solar Current",
                           unit="A", device_class="current",
                           state_class=STATE_CLASS_MEASUREMENT, decimals=3,
                           entity_category=ENTITY_CATEGORY_DIAGNOSTIC)
    cg.add(var.set_solar_current_sensor(s))

    s = await _make_sensor(config["solar_voltage_id"], f"{p} Solar Voltage",
                           unit="V", device_class="voltage",
                           state_class=STATE_CLASS_MEASUREMENT, decimals=3,
                           entity_category=ENTITY_CATEGORY_DIAGNOSTIC)
    cg.add(var.set_solar_voltage_sensor(s))

    s = await _make_sensor(config["solar_power_id"], f"{p} Solar Power",
                           unit="W", device_class="power",
                           state_class=STATE_CLASS_MEASUREMENT, decimals=1)
    cg.add(var.set_solar_power_sensor(s))

    s = await _make_sensor(config["solar_energy_id"], f"{p} Solar Energy",
                           unit="Wh", device_class="energy",
                           state_class=STATE_CLASS_TOTAL_INCREASING, decimals=0)
    cg.add(var.set_solar_energy_sensor(s))

    s = await _make_sensor(config["ac_input_voltage_id"], f"{p} AC Input Voltage",
                           unit="V", device_class="voltage",
                           state_class=STATE_CLASS_MEASUREMENT, decimals=0)
    cg.add(var.set_ac_input_voltage_sensor(s))
