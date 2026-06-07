import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, switch
from esphome.const import CONF_ID

CODEOWNERS = ["@dev8edss"]
AUTO_LOAD = ["sensor", "switch", "redarc_common"]
MULTI_CONF = False

CONF_SOURCE_ADDRESS = "source_address"
CONF_HOST_ADDRESS = "host_address"

ns = cg.esphome_ns.namespace("tvms_1280")
TVMS1280Component = ns.class_("TVMS1280Component", cg.Component)
TVMS1280Switch = ns.class_("TVMS1280Switch", switch.Switch)

_sensor_ns = cg.esphome_ns.namespace("sensor")
_SensorClass = _sensor_ns.class_("Sensor")

_AUTO_IDS = {}
for _i in range(1, 7):
    _AUTO_IDS[cv.GenerateID(f"tank_{_i}_id")] = cv.declare_id(_SensorClass)
for _i in range(10):
    _AUTO_IDS[cv.GenerateID(f"output_{_i}_id")] = cv.declare_id(TVMS1280Switch)
_AUTO_IDS[cv.GenerateID("temp1_id")] = cv.declare_id(_SensorClass)
_AUTO_IDS[cv.GenerateID("temp2_id")] = cv.declare_id(_SensorClass)
_AUTO_IDS[cv.GenerateID("supply_voltage_id")] = cv.declare_id(_SensorClass)
_AUTO_IDS[cv.GenerateID("voltage_input1_id")] = cv.declare_id(_SensorClass)
_AUTO_IDS[cv.GenerateID("voltage_input2_id")] = cv.declare_id(_SensorClass)
_AUTO_IDS[cv.GenerateID("last_cmd_channel_id")] = cv.declare_id(_SensorClass)
_AUTO_IDS[cv.GenerateID("last_cmd_state_id")] = cv.declare_id(_SensorClass)
_AUTO_IDS[cv.GenerateID("inverter_id")] = cv.declare_id(TVMS1280Switch)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(TVMS1280Component),
    cv.Optional(CONF_SOURCE_ADDRESS, default=0x24): cv.hex_uint8_t,
    cv.Optional(CONF_HOST_ADDRESS, default=0x20): cv.hex_uint8_t,
    **_AUTO_IDS,
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
    cg.add(var.set_host_address(config[CONF_HOST_ADDRESS]))

    p = config[CONF_ID].id.replace("_", " ")
    SC = "sensor::StateClass::STATE_CLASS_MEASUREMENT"
    DIAG = "ENTITY_CATEGORY_DIAGNOSTIC"

    s = _make_sensor(config["temp1_id"], f"{p} Temperature 1",
                     unit="°C", device_class="temperature", state_class_raw=SC, decimals=0)
    cg.add(var.set_temp1_sensor(s))

    s = _make_sensor(config["temp2_id"], f"{p} Temperature 2",
                     unit="°C", device_class="temperature", state_class_raw=SC, decimals=0)
    cg.add(var.set_temp2_sensor(s))

    s = _make_sensor(config["supply_voltage_id"], f"{p} Supply Voltage",
                     unit="V", device_class="voltage", state_class_raw=SC, decimals=2,
                     entity_category_raw=DIAG)
    cg.add(var.set_supply_voltage_sensor(s))

    s = _make_sensor(config["voltage_input1_id"], f"{p} Voltage Input 1 Candidate",
                     unit="V", device_class="voltage", state_class_raw=SC, decimals=1,
                     entity_category_raw=DIAG)
    cg.add(var.set_voltage_input1_sensor(s))

    s = _make_sensor(config["voltage_input2_id"], f"{p} Voltage Input 2 Candidate",
                     unit="V", device_class="voltage", state_class_raw=SC, decimals=1,
                     entity_category_raw=DIAG)
    cg.add(var.set_voltage_input2_sensor(s))

    s = _make_sensor(config["last_cmd_channel_id"], f"{p} Last Output Command Channel",
                     state_class_raw=SC, decimals=0, entity_category_raw=DIAG)
    cg.add(var.set_last_command_channel_sensor(s))

    s = _make_sensor(config["last_cmd_state_id"], f"{p} Last Output Command State",
                     state_class_raw=SC, decimals=0, entity_category_raw=DIAG)
    cg.add(var.set_last_command_state_sensor(s))

    for i in range(1, 7):
        s = _make_sensor(config[f"tank_{i}_id"], f"{p} Tank {i}",
                         unit="%", state_class_raw=SC, decimals=0)
        cg.add(var.set_tank_sensor(i, s))

    # Output switches (channels 0x04–0x0D)
    for i in range(10):
        sw = cg.new_Pvariable(config[f"output_{i}_id"])
        cg.add(sw.set_name(f"{p} Output {i}"))
        cg.add(sw.set_parent(var))
        cg.add(sw.set_output_number(i))
        cg.add(sw.set_channel(0x04 + i))
        cg.add(sw.set_is_inverter(False))
        cg.add(cg.App.register_component(sw))
        cg.add(cg.App.register_switch(sw))
        cg.add(var.register_output_switch(sw))

    # Inverter switch (channel 0x0E)
    inv = cg.new_Pvariable(config["inverter_id"])
    cg.add(inv.set_name(f"{p} Inverter"))
    cg.add(inv.set_parent(var))
    cg.add(inv.set_output_number(0))
    cg.add(inv.set_channel(0x0E))
    cg.add(inv.set_is_inverter(True))
    cg.add(cg.App.register_component(inv))
    cg.add(cg.App.register_switch(inv))
    cg.add(var.register_inverter_switch(inv))
