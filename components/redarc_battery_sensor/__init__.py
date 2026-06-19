import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_ACCURACY_DECIMALS, CONF_DEVICE_CLASS, CONF_DISABLED_BY_DEFAULT,
    CONF_ENTITY_CATEGORY, CONF_FORCE_UPDATE, CONF_ICON, CONF_ID, CONF_NAME,
    CONF_STATE_CLASS, CONF_UNIT_OF_MEASUREMENT,
    ENTITY_CATEGORY_DIAGNOSTIC, ENTITY_CATEGORY_NONE,
    STATE_CLASS_MEASUREMENT,
)

CODEOWNERS = ["@dev8edss"]
AUTO_LOAD = ["sensor", "redarc_common"]
MULTI_CONF = False

CONF_SOURCE_ADDRESS = "source_address"
CONF_FILTER_INTERVAL = "filter_interval"

battery_sensor_ns = cg.esphome_ns.namespace("redarc_battery_sensor")
BatterySensorComponent = battery_sensor_ns.class_("BatterySensorComponent", cg.Component)

_sensor_ns = cg.esphome_ns.namespace("sensor")
_SensorClass = _sensor_ns.class_("Sensor")
_StateClass = _sensor_ns.enum("StateClass")
_SC_MAP = {
    "measurement": _StateClass.STATE_CLASS_MEASUREMENT,
    "total_increasing": _StateClass.STATE_CLASS_TOTAL_INCREASING,
}

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(BatterySensorComponent),
    cv.Optional(CONF_SOURCE_ADDRESS, default=0x08): cv.hex_uint8_t,
    cv.Optional(CONF_FILTER_INTERVAL, default="5s"): cv.positive_time_period_milliseconds,
    cv.GenerateID("current_id"): cv.declare_id(_SensorClass),
    cv.GenerateID("current_raw_id"): cv.declare_id(_SensorClass),
    cv.GenerateID("voltage_id"): cv.declare_id(_SensorClass),
    cv.GenerateID("temperature_id"): cv.declare_id(_SensorClass),
    cv.GenerateID("soc_id"): cv.declare_id(_SensorClass),
    cv.GenerateID("battery_type_id"): cv.declare_id(_SensorClass),
    cv.GenerateID("configured_capacity_id"): cv.declare_id(_SensorClass),
    cv.GenerateID("max_charge_current_id"): cv.declare_id(_SensorClass),
    cv.GenerateID("low_soc_alarm_id"): cv.declare_id(_SensorClass),
    cv.GenerateID("low_voltage_alarm_id"): cv.declare_id(_SensorClass),
    cv.GenerateID("last_soc_calibration_target_id"): cv.declare_id(_SensorClass),
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
        cfg[CONF_STATE_CLASS] = _SC_MAP.get(state_class, state_class)
    if decimals is not None:
        cfg[CONF_ACCURACY_DECIMALS] = decimals
    return await sensor.new_sensor(cfg)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_source_address(config[CONF_SOURCE_ADDRESS]))
    cg.add(var.set_filter_interval_ms(config[CONF_FILTER_INTERVAL]))

    p = config[CONF_ID].id.replace("_", " ")

    s = await _make_sensor(config["current_id"], f"{p} Current",
                           unit="A", device_class="current",
                           state_class=STATE_CLASS_MEASUREMENT, decimals=3)
    cg.add(var.set_current_sensor(s))

    s = await _make_sensor(config["current_raw_id"], f"{p} Current Raw",
                           state_class=STATE_CLASS_MEASUREMENT, decimals=0,
                           entity_category=ENTITY_CATEGORY_DIAGNOSTIC)
    cg.add(var.set_current_raw_sensor(s))

    s = await _make_sensor(config["voltage_id"], f"{p} Voltage",
                           unit="V", device_class="voltage",
                           state_class=STATE_CLASS_MEASUREMENT, decimals=3)
    cg.add(var.set_voltage_sensor(s))

    s = await _make_sensor(config["temperature_id"], f"{p} Temperature",
                           unit="°C", device_class="temperature",
                           state_class=STATE_CLASS_MEASUREMENT, decimals=0)
    cg.add(var.set_temperature_sensor(s))

    s = await _make_sensor(config["soc_id"], f"{p} SOC",
                           unit="%", device_class="battery",
                           state_class=STATE_CLASS_MEASUREMENT, decimals=0)
    cg.add(var.set_soc_sensor(s))

    s = await _make_sensor(config["battery_type_id"], f"{p} Battery Type",
                           state_class=STATE_CLASS_MEASUREMENT, decimals=0,
                           entity_category=ENTITY_CATEGORY_DIAGNOSTIC)
    cg.add(var.set_battery_type_sensor(s))

    s = await _make_sensor(config["configured_capacity_id"], f"{p} Configured Capacity",
                           unit="Ah", state_class=STATE_CLASS_MEASUREMENT, decimals=0,
                           entity_category=ENTITY_CATEGORY_DIAGNOSTIC)
    cg.add(var.set_configured_capacity_sensor(s))

    s = await _make_sensor(config["max_charge_current_id"], f"{p} Max Charge Current",
                           unit="A", device_class="current",
                           state_class=STATE_CLASS_MEASUREMENT, decimals=0,
                           entity_category=ENTITY_CATEGORY_DIAGNOSTIC)
    cg.add(var.set_max_charge_current_sensor(s))

    s = await _make_sensor(config["low_soc_alarm_id"], f"{p} Low SOC Alarm",
                           unit="%", device_class="battery",
                           state_class=STATE_CLASS_MEASUREMENT, decimals=0,
                           entity_category=ENTITY_CATEGORY_DIAGNOSTIC)
    cg.add(var.set_low_soc_alarm_sensor(s))

    s = await _make_sensor(config["low_voltage_alarm_id"], f"{p} Low Voltage Alarm",
                           unit="V", device_class="voltage",
                           state_class=STATE_CLASS_MEASUREMENT, decimals=1,
                           entity_category=ENTITY_CATEGORY_DIAGNOSTIC)
    cg.add(var.set_low_voltage_alarm_sensor(s))

    s = await _make_sensor(config["last_soc_calibration_target_id"], f"{p} Last SOC Calibration Target",
                           unit="%", device_class="battery",
                           state_class=STATE_CLASS_MEASUREMENT, decimals=0,
                           entity_category=ENTITY_CATEGORY_DIAGNOSTIC)
    cg.add(var.set_last_soc_calibration_target_sensor(s))
