import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import CONF_ID

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
    SC = "sensor::StateClass::STATE_CLASS_MEASUREMENT"
    SC_INC = "sensor::StateClass::STATE_CLASS_TOTAL_INCREASING"
    DIAG = "ENTITY_CATEGORY_DIAGNOSTIC"

    s = _make_sensor(config["output_current_id"], f"{p} Output Current",
                     unit="A", device_class="current", state_class_raw=SC, decimals=3)
    cg.add(var.set_output_current_sensor(s))

    s = _make_sensor(config["output_current_raw_id"], f"{p} Output Current Raw",
                     state_class_raw=SC, decimals=0, entity_category_raw=DIAG)
    cg.add(var.set_output_current_raw_sensor(s))

    s = _make_sensor(config["battery_voltage_id"], f"{p} Battery Voltage",
                     unit="V", device_class="voltage", state_class_raw=SC, decimals=3)
    cg.add(var.set_battery_voltage_sensor(s))

    s = _make_sensor(config["solar_current_id"], f"{p} Solar Current",
                     unit="A", device_class="current", state_class_raw=SC, decimals=3,
                     entity_category_raw=DIAG)
    cg.add(var.set_solar_current_sensor(s))

    s = _make_sensor(config["solar_voltage_id"], f"{p} Solar Voltage",
                     unit="V", device_class="voltage", state_class_raw=SC, decimals=3,
                     entity_category_raw=DIAG)
    cg.add(var.set_solar_voltage_sensor(s))

    s = _make_sensor(config["solar_power_id"], f"{p} Solar Power",
                     unit="W", device_class="power", state_class_raw=SC, decimals=1)
    cg.add(var.set_solar_power_sensor(s))

    s = _make_sensor(config["solar_energy_id"], f"{p} Solar Energy",
                     unit="Wh", device_class="energy", state_class_raw=SC_INC, decimals=0)
    cg.add(var.set_solar_energy_sensor(s))

    s = _make_sensor(config["ac_input_voltage_id"], f"{p} AC Input Voltage",
                     unit="V", device_class="voltage", state_class_raw=SC, decimals=0)
    cg.add(var.set_ac_input_voltage_sensor(s))
