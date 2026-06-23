import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import button, number, select, sensor, text_sensor
from esphome.const import (
    CONF_ACCURACY_DECIMALS, CONF_DEVICE_CLASS, CONF_DISABLED_BY_DEFAULT,
    CONF_ENTITY_CATEGORY, CONF_FORCE_UPDATE, CONF_ICON, CONF_ID, CONF_MODE,
    CONF_NAME, CONF_STATE_CLASS, CONF_UNIT_OF_MEASUREMENT,
    ENTITY_CATEGORY_DIAGNOSTIC, ENTITY_CATEGORY_NONE,
    STATE_CLASS_MEASUREMENT,
)

CODEOWNERS = ["@dev8edss"]
AUTO_LOAD = ["button", "number", "select", "sensor", "text_sensor", "redarc_common"]
MULTI_CONF = False

CONF_SOURCE_ADDRESS = "source_address"
CONF_HOST_ADDRESS = "host_address"
CONF_FILTER_INTERVAL = "filter_interval"
CONF_SOC_HISTORY_POLL_INTERVAL = "soc_history_poll_interval"


def zero_or_positive_time_period_milliseconds(value):
    if value in (0, "0", "0s", "0ms"):
        return 0
    return cv.positive_time_period_milliseconds(value)

battery_sensor_ns = cg.esphome_ns.namespace("redarc_battery_sensor")
BatterySensorComponent = battery_sensor_ns.class_("BatterySensorComponent", cg.Component)
BatterySOCCalibrateButton = battery_sensor_ns.class_("BatterySOCCalibrateButton", button.Button)
BatteryConfigNumber = battery_sensor_ns.class_("BatteryConfigNumber", number.Number)
BatteryTypeSelect = battery_sensor_ns.class_("BatteryTypeSelect", select.Select)

_sensor_ns = cg.esphome_ns.namespace("sensor")
_SensorClass = _sensor_ns.class_("Sensor")
_StateClass = _sensor_ns.enum("StateClass")
_SC_MAP = {
    "measurement": _StateClass.STATE_CLASS_MEASUREMENT,
    "total_increasing": _StateClass.STATE_CLASS_TOTAL_INCREASING,
}
_text_sensor_ns = cg.esphome_ns.namespace("text_sensor")
_TextSensorClass = _text_sensor_ns.class_("TextSensor")

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(BatterySensorComponent),
    cv.Optional(CONF_SOURCE_ADDRESS, default=0x08): cv.hex_uint8_t,
    cv.Optional(CONF_HOST_ADDRESS, default=0x20): cv.hex_uint8_t,
    cv.Optional(CONF_FILTER_INTERVAL, default="5s"): cv.positive_time_period_milliseconds,
    cv.Optional(CONF_SOC_HISTORY_POLL_INTERVAL, default="60s"): zero_or_positive_time_period_milliseconds,
    cv.GenerateID("current_id"): cv.declare_id(_SensorClass),
    cv.GenerateID("voltage_id"): cv.declare_id(_SensorClass),
    cv.GenerateID("temperature_id"): cv.declare_id(_SensorClass),
    cv.GenerateID("soc_id"): cv.declare_id(_SensorClass),
    cv.GenerateID("last_soc_calibration_target_id"): cv.declare_id(_SensorClass),
    cv.GenerateID("battery_type_select_id"): cv.declare_id(BatteryTypeSelect),
    cv.GenerateID("configured_capacity_number_id"): cv.declare_id(BatteryConfigNumber),
    cv.GenerateID("max_charge_current_number_id"): cv.declare_id(BatteryConfigNumber),
    cv.GenerateID("low_soc_alarm_number_id"): cv.declare_id(BatteryConfigNumber),
    cv.GenerateID("low_voltage_alarm_number_id"): cv.declare_id(BatteryConfigNumber),
    cv.GenerateID("soc_calibration_button_id"): cv.declare_id(BatterySOCCalibrateButton),
    cv.GenerateID("soc_hourly_history_id"): cv.declare_id(_TextSensorClass),
    cv.GenerateID("soc_daily_range_history_id"): cv.declare_id(_TextSensorClass),
}).extend(cv.COMPONENT_SCHEMA)


async def _make_sensor(config_id, name, unit=None, device_class=None,
                       state_class=None, decimals=None, entity_category=None,
                       force_update=False):
    cfg = {
        CONF_ID: config_id,
        CONF_NAME: name,
        CONF_DISABLED_BY_DEFAULT: False,
        CONF_FORCE_UPDATE: force_update,
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


async def _make_number(config_id, name, min_value, max_value, step,
                       unit=None, device_class=None):
    cfg = {
        CONF_ID: config_id,
        CONF_NAME: name,
        CONF_DISABLED_BY_DEFAULT: False,
        CONF_FORCE_UPDATE: False,
        CONF_ICON: "",
        CONF_ENTITY_CATEGORY: ENTITY_CATEGORY_NONE,
        CONF_MODE: number.NUMBER_MODES["SLIDER"],
    }
    if unit is not None:
        cfg[CONF_UNIT_OF_MEASUREMENT] = unit
    if device_class is not None:
        cfg[CONF_DEVICE_CLASS] = device_class
    return await number.new_number(cfg, min_value=min_value, max_value=max_value, step=step)


async def _make_select(config_id, name, options):
    cfg = {
        CONF_ID: config_id,
        CONF_NAME: name,
        CONF_DISABLED_BY_DEFAULT: False,
        CONF_ICON: "",
        CONF_ENTITY_CATEGORY: ENTITY_CATEGORY_NONE,
    }
    return await select.new_select(cfg, options=options)


async def _make_button(config_id, name):
    cfg = {
        CONF_ID: config_id,
        CONF_NAME: name,
        CONF_DISABLED_BY_DEFAULT: False,
        CONF_ICON: "mdi:battery-sync",
        CONF_ENTITY_CATEGORY: ENTITY_CATEGORY_NONE,
    }
    return await button.new_button(cfg)


async def _make_text_sensor(config_id, name, entity_category=None):
    cfg = {
        CONF_ID: config_id,
        CONF_NAME: name,
        CONF_DISABLED_BY_DEFAULT: False,
        CONF_ICON: "",
        CONF_ENTITY_CATEGORY: entity_category if entity_category is not None else ENTITY_CATEGORY_NONE,
    }
    return await text_sensor.new_text_sensor(cfg)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_source_address(config[CONF_SOURCE_ADDRESS]))
    cg.add(var.set_host_address(config[CONF_HOST_ADDRESS]))
    cg.add(var.set_filter_interval_ms(config[CONF_FILTER_INTERVAL]))
    cg.add(var.set_soc_history_poll_interval_ms(config[CONF_SOC_HISTORY_POLL_INTERVAL]))

    p = config[CONF_ID].id.replace("_", " ")

    s = await _make_sensor(config["current_id"], f"{p} Current",
                           unit="A", device_class="current",
                           state_class=STATE_CLASS_MEASUREMENT, decimals=3)
    cg.add(var.set_current_sensor(s))

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
                           state_class=STATE_CLASS_MEASUREMENT, decimals=0,
                           force_update=True)
    cg.add(var.set_soc_sensor(s))

    s = await _make_sensor(config["last_soc_calibration_target_id"], f"{p} Last SOC Calibration Target",
                           unit="%", device_class="battery",
                           state_class=STATE_CLASS_MEASUREMENT, decimals=0,
                           entity_category=ENTITY_CATEGORY_DIAGNOSTIC)
    cg.add(var.set_last_soc_calibration_target_sensor(s))

    sel = await _make_select(config["battery_type_select_id"], f"{p} Battery Type",
                             ["Gel", "AGM", "LeadAcid", "Calcium", "Lithium"])
    cg.add(sel.set_parent(var))
    cg.add(var.set_battery_type_select(sel))

    n = await _make_number(config["configured_capacity_number_id"], f"{p} Configured Capacity",
                           min_value=1, max_value=1000, step=1, unit="Ah")
    cg.add(n.set_parent(var))
    cg.add(n.set_command(0x12))
    cg.add(var.set_configured_capacity_number(n))

    n = await _make_number(config["max_charge_current_number_id"], f"{p} Max Charge Current",
                           min_value=0, max_value=100, step=1, unit="A", device_class="current")
    cg.add(n.set_parent(var))
    cg.add(n.set_command(0x14))
    cg.add(var.set_max_charge_current_number(n))

    n = await _make_number(config["low_soc_alarm_number_id"], f"{p} Low SOC Alarm",
                           min_value=0, max_value=100, step=1, unit="%", device_class="battery")
    cg.add(n.set_parent(var))
    cg.add(n.set_command(0x41))
    cg.add(var.set_low_soc_alarm_number(n))

    n = await _make_number(config["low_voltage_alarm_number_id"], f"{p} Low Voltage Alarm",
                           min_value=0, max_value=16, step=0.1, unit="V", device_class="voltage")
    cg.add(n.set_parent(var))
    cg.add(n.set_command(0x42))
    cg.add(n.set_raw_multiplier(1000.0))
    cg.add(var.set_low_voltage_alarm_number(n))

    b = await _make_button(config["soc_calibration_button_id"], f"{p} Calibrate SOC Full")
    cg.add(b.set_parent(var))
    cg.add(var.set_soc_calibration_button(b))

    ts = await _make_text_sensor(config["soc_hourly_history_id"], f"{p} SOC Hourly History",
                                 entity_category=ENTITY_CATEGORY_DIAGNOSTIC)
    cg.add(var.set_soc_hourly_history_text_sensor(ts))

    ts = await _make_text_sensor(config["soc_daily_range_history_id"], f"{p} SOC Daily Range History",
                                 entity_category=ENTITY_CATEGORY_DIAGNOSTIC)
    cg.add(var.set_soc_daily_range_history_text_sensor(ts))
