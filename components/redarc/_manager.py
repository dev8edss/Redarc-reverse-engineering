import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import button, select, sensor, text_sensor, time
from esphome.const import (
    CONF_ACCURACY_DECIMALS, CONF_DEVICE_CLASS, CONF_DISABLED_BY_DEFAULT,
    CONF_ENTITY_CATEGORY, CONF_FORCE_UPDATE, CONF_ICON, CONF_ID, CONF_NAME,
    CONF_STATE_CLASS, CONF_UNIT_OF_MEASUREMENT,
    ENTITY_CATEGORY_DIAGNOSTIC, ENTITY_CATEGORY_NONE,
    STATE_CLASS_MEASUREMENT,
)

from ._battery_sensor import BatterySensorComponent as _BatterySensorComponent

CONF_SOURCE_ADDRESS = "source_address"
CONF_HOST_ADDRESS = "host_address"
CONF_FILTER_INTERVAL = "filter_interval"
CONF_BATTERY_SENSOR = "battery_sensor_id"
CONF_SOLAR_HISTORY_POLL_INTERVAL = "solar_history_poll_interval"
CONF_TIME_ID = "time_id"


def zero_or_positive_time_period_milliseconds(value):
    if value in (0, "0", "0s", "0ms"):
        return 0
    return cv.positive_time_period_milliseconds(value)

manager30_ns = cg.esphome_ns.namespace("redarc_manager")
Manager30Component = manager30_ns.class_("Manager30Component", cg.Component)
VehicleInputTriggerSelect = manager30_ns.class_("VehicleInputTriggerSelect", select.Select)
ChargingModeSelect = manager30_ns.class_("ChargingModeSelect", select.Select)
Manager30SetClockButton = manager30_ns.class_("Manager30SetClockButton", button.Button)
Manager30ClearSolarHistoryButton = manager30_ns.class_("Manager30ClearSolarHistoryButton", button.Button)

_sensor_ns = cg.esphome_ns.namespace("sensor")
_SensorClass = _sensor_ns.class_("Sensor")
_StateClass = _sensor_ns.enum("StateClass")
_SC_MAP = {
    "measurement": _StateClass.STATE_CLASS_MEASUREMENT,
    "total_increasing": _StateClass.STATE_CLASS_TOTAL_INCREASING,
}

_text_sensor_ns = cg.esphome_ns.namespace("text_sensor")
_TextSensorClass = _text_sensor_ns.class_("TextSensor")

SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(Manager30Component),
    cv.Optional(CONF_SOURCE_ADDRESS, default=0x01): cv.hex_uint8_t,
    cv.Optional(CONF_HOST_ADDRESS): cv.hex_uint8_t,
    cv.Optional(CONF_FILTER_INTERVAL): cv.positive_time_period_milliseconds,
    cv.Optional(CONF_SOLAR_HISTORY_POLL_INTERVAL): zero_or_positive_time_period_milliseconds,
    cv.Optional(CONF_BATTERY_SENSOR): cv.use_id(_BatterySensorComponent),
    cv.Optional(CONF_TIME_ID): cv.use_id(time.RealTimeClock),
    cv.GenerateID("set_clock_button_id"): cv.declare_id(Manager30SetClockButton),
    cv.GenerateID("clear_solar_history_button_id"): cv.declare_id(Manager30ClearSolarHistoryButton),
    cv.GenerateID("output_current_id"): cv.declare_id(_SensorClass),
    cv.GenerateID("battery_voltage_id"): cv.declare_id(_SensorClass),
    cv.GenerateID("source_device_current_id"): cv.declare_id(_SensorClass),
    cv.GenerateID("solar_current_id"): cv.declare_id(_SensorClass),
    cv.GenerateID("solar_voltage_id"): cv.declare_id(_SensorClass),
    cv.GenerateID("solar_power_id"): cv.declare_id(_SensorClass),
    cv.GenerateID("solar_energy_id"): cv.declare_id(_SensorClass),
    cv.GenerateID("solar_day_history_id"): cv.declare_id(_TextSensorClass),
    cv.GenerateID("ac_input_current_id"): cv.declare_id(_SensorClass),
    cv.GenerateID("ac_input_voltage_id"): cv.declare_id(_SensorClass),
    cv.GenerateID("vehicle_input_current_id"): cv.declare_id(_SensorClass),
    cv.GenerateID("vehicle_input_voltage_id"): cv.declare_id(_SensorClass),
    cv.GenerateID("clock_date_id"): cv.declare_id(_TextSensorClass),
    cv.GenerateID("clock_time_id"): cv.declare_id(_TextSensorClass),
    cv.GenerateID("clock_datetime_id"): cv.declare_id(_TextSensorClass),
    cv.GenerateID("charging_stage_id"): cv.declare_id(_TextSensorClass),
    cv.GenerateID("vehicle_input_trigger_select_id"): cv.declare_id(VehicleInputTriggerSelect),
    cv.GenerateID("charging_mode_select_id"): cv.declare_id(ChargingModeSelect),
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


async def _make_select(config_id, name, options):
    cfg = {
        CONF_ID: config_id,
        CONF_NAME: name,
        CONF_DISABLED_BY_DEFAULT: False,
        CONF_ICON: "",
        CONF_ENTITY_CATEGORY: ENTITY_CATEGORY_NONE,
    }
    return await select.new_select(cfg, options=options)


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
    cg.add(var.set_solar_history_poll_interval_ms(config[CONF_SOLAR_HISTORY_POLL_INTERVAL]))

    if CONF_BATTERY_SENSOR in config:
        battery = await cg.get_variable(config[CONF_BATTERY_SENSOR])
        cg.add(var.set_battery_sensor(battery))

    p = config[CONF_ID].id.replace("_", " ")

    s = await _make_sensor(config["output_current_id"], f"{p} Output Current",
                           unit="A", device_class="current",
                           state_class=STATE_CLASS_MEASUREMENT, decimals=3)
    cg.add(var.set_output_current_sensor(s))

    s = await _make_sensor(config["battery_voltage_id"], f"{p} Battery Voltage",
                           unit="V", device_class="voltage",
                           state_class=STATE_CLASS_MEASUREMENT, decimals=3)
    cg.add(var.set_battery_voltage_sensor(s))

    s = await _make_sensor(config["source_device_current_id"], f"{p} Device Current",
                           unit="A", device_class="current",
                           state_class=STATE_CLASS_MEASUREMENT, decimals=3)
    cg.add(var.set_source_device_current_sensor(s))

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
                           state_class=STATE_CLASS_MEASUREMENT, decimals=0)
    cg.add(var.set_solar_energy_sensor(s))

    ts = await _make_text_sensor(config["solar_day_history_id"], f"{p} Solar Day -1..-11 History",
                                 entity_category=ENTITY_CATEGORY_DIAGNOSTIC)
    cg.add(var.set_solar_day_history_text_sensor(ts))

    s = await _make_sensor(config["ac_input_current_id"], f"{p} AC Input Current",
                           unit="A", device_class="current",
                           state_class=STATE_CLASS_MEASUREMENT, decimals=3)
    cg.add(var.set_ac_input_current_sensor(s))

    s = await _make_sensor(config["ac_input_voltage_id"], f"{p} AC Input Voltage",
                           unit="V", device_class="voltage",
                           state_class=STATE_CLASS_MEASUREMENT, decimals=0)
    cg.add(var.set_ac_input_voltage_sensor(s))

    s = await _make_sensor(config["vehicle_input_current_id"], f"{p} Vehicle Input Current",
                           unit="A", device_class="current",
                           state_class=STATE_CLASS_MEASUREMENT, decimals=3)
    cg.add(var.set_vehicle_input_current_sensor(s))

    s = await _make_sensor(config["vehicle_input_voltage_id"], f"{p} Vehicle Input Voltage",
                           unit="V", device_class="voltage",
                           state_class=STATE_CLASS_MEASUREMENT, decimals=3)
    cg.add(var.set_vehicle_input_voltage_sensor(s))

    ts = await _make_text_sensor(config["clock_date_id"], f"{p} CAN Date")
    cg.add(var.set_clock_date_text_sensor(ts))

    ts = await _make_text_sensor(config["clock_time_id"], f"{p} CAN Time")
    cg.add(var.set_clock_time_text_sensor(ts))

    ts = await _make_text_sensor(config["clock_datetime_id"], f"{p} CAN Date Time")
    cg.add(var.set_clock_datetime_text_sensor(ts))

    ts = await _make_text_sensor(config["charging_stage_id"], f"{p} Charging Stage")
    cg.add(var.set_charging_stage_text_sensor(ts))

    sel = await _make_select(config["vehicle_input_trigger_select_id"], f"{p} Vehicle Input Trigger",
                             ["Auto", "12V", "24V", "Ignition", "On"])
    cg.add(sel.set_parent(var))
    cg.add(var.set_vehicle_input_trigger_select(sel))

    sel = await _make_select(config["charging_mode_select_id"], f"{p} Charging Mode",
                             ["Touring", "Storage"])
    cg.add(sel.set_parent(var))
    cg.add(var.set_charging_mode_select(sel))

    if CONF_TIME_ID in config:
        t = await cg.get_variable(config[CONF_TIME_ID])
        cg.add(var.set_time_source(t))

    btn = await button.new_button({
        CONF_ID: config["set_clock_button_id"],
        CONF_NAME: f"{p} Set Time",
        CONF_DISABLED_BY_DEFAULT: False,
        CONF_ICON: "mdi:clock-edit",
        CONF_ENTITY_CATEGORY: ENTITY_CATEGORY_DIAGNOSTIC,
    })
    cg.add(btn.set_parent(var))
    cg.add(var.set_set_clock_button(btn))

    btn = await button.new_button({
        CONF_ID: config["clear_solar_history_button_id"],
        CONF_NAME: f"{p} Delete Solar History",
        CONF_DISABLED_BY_DEFAULT: False,
        CONF_ICON: "mdi:delete-clock",
        CONF_ENTITY_CATEGORY: ENTITY_CATEGORY_DIAGNOSTIC,
    })
    cg.add(btn.set_parent(var))
    cg.add(var.set_clear_solar_history_button(btn))

    return var
