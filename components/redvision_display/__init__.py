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
MULTI_CONF = True

CONF_SOURCE_ADDRESS = "source_address"
CONF_FILTER_INTERVAL = "filter_interval"

ns = cg.esphome_ns.namespace("redvision_display")
RedvisionDisplayComponent = ns.class_("RedvisionDisplayComponent", cg.Component)

_sensor_ns = cg.esphome_ns.namespace("sensor")
_SensorClass = _sensor_ns.class_("Sensor")
_StateClass = _sensor_ns.enum("StateClass")
_SC_MAP = {
    "measurement": _StateClass.STATE_CLASS_MEASUREMENT,
    "total_increasing": _StateClass.STATE_CLASS_TOTAL_INCREASING,
}

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(RedvisionDisplayComponent),
    cv.Required(CONF_SOURCE_ADDRESS): cv.hex_uint8_t,
    cv.Optional(CONF_FILTER_INTERVAL, default="5s"): cv.positive_time_period_milliseconds,
    cv.GenerateID("batt_current_id"): cv.declare_id(_SensorClass),
    cv.GenerateID("device_current_id"): cv.declare_id(_SensorClass),
    cv.GenerateID("batt_current_raw_id"): cv.declare_id(_SensorClass),
    cv.GenerateID("device_current_raw_id"): cv.declare_id(_SensorClass),
    cv.GenerateID("mgr_output_current_id"): cv.declare_id(_SensorClass),
    cv.GenerateID("mgr_output_current_raw_id"): cv.declare_id(_SensorClass),
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
    cg.add(var.set_display_type(1))

    p = config[CONF_ID].id.replace("_", " ")

    s = await _make_sensor(config["batt_current_id"], f"{p} Battery Current Display",
                           unit="A", device_class="current",
                           state_class=STATE_CLASS_MEASUREMENT, decimals=1)
    cg.add(var.set_battery_current_display_sensor(s))

    s = await _make_sensor(config["device_current_id"], f"{p} Device Current Display",
                           unit="A", device_class="current",
                           state_class=STATE_CLASS_MEASUREMENT, decimals=1)
    cg.add(var.set_device_current_display_sensor(s))

    s = await _make_sensor(config["batt_current_raw_id"], f"{p} Battery Current Display Raw",
                           state_class=STATE_CLASS_MEASUREMENT, decimals=0,
                           entity_category=ENTITY_CATEGORY_DIAGNOSTIC)
    cg.add(var.set_battery_current_display_raw_sensor(s))

    s = await _make_sensor(config["device_current_raw_id"], f"{p} Device Current Display Raw",
                           state_class=STATE_CLASS_MEASUREMENT, decimals=0,
                           entity_category=ENTITY_CATEGORY_DIAGNOSTIC)
    cg.add(var.set_device_current_display_raw_sensor(s))

    s = await _make_sensor(config["mgr_output_current_id"], f"{p} Manager Output Current Display",
                           unit="A", device_class="current",
                           state_class=STATE_CLASS_MEASUREMENT, decimals=1)
    cg.add(var.set_manager_output_current_display_sensor(s))

    s = await _make_sensor(config["mgr_output_current_raw_id"], f"{p} Manager Output Current Display Raw",
                           state_class=STATE_CLASS_MEASUREMENT, decimals=0,
                           entity_category=ENTITY_CATEGORY_DIAGNOSTIC)
    cg.add(var.set_manager_output_current_display_raw_sensor(s))
