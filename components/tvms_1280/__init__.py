import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import canbus, sensor, switch
from esphome.const import CONF_ID, CONF_NAME

CODEOWNERS = ["@dev8edss"]
DEPENDENCIES = ["canbus"]
AUTO_LOAD = ["sensor", "switch", "redarc_common"]
MULTI_CONF = False

CONF_CANBUS_ID = "canbus_id"
CONF_SOURCE_ADDRESS = "source_address"
CONF_COMMAND_ID = "command_id"
CONF_TEMP_TANK_ID = "temp_tank_id"
CONF_OUTPUT_STATUS_ID = "output_status_id"
CONF_CHANNEL_STATUS_ID = "channel_status_id"
CONF_OUTPUTS = "outputs"
CONF_INVERTER = "inverter"
CONF_OUTPUT_NUMBER = "output_number"
CONF_CHANNEL = "channel"
CONF_TEMP1 = "temp1"
CONF_TEMP2 = "temp2"
CONF_TANKS = "tanks"
CONF_LAST_COMMAND_CHANNEL = "last_command_channel"
CONF_LAST_COMMAND_STATE = "last_command_state"

ns = cg.esphome_ns.namespace("tvms_1280")
TVMS1280Component = ns.class_("TVMS1280Component", cg.Component)
TVMS1280Switch = ns.class_("TVMS1280Switch", switch.Switch)

OUTPUT_SCHEMA = switch.switch_schema(TVMS1280Switch).extend({
    cv.Required(CONF_OUTPUT_NUMBER): cv.int_range(min=1, max=10),
    cv.Optional(CONF_CHANNEL): cv.hex_uint8_t,
})

INVERTER_SCHEMA = switch.switch_schema(TVMS1280Switch).extend({
    cv.Optional(CONF_CHANNEL, default=0x0E): cv.hex_uint8_t,
})

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(TVMS1280Component),
    cv.Required(CONF_CANBUS_ID): cv.use_id(canbus.CanbusComponent),
    cv.Optional(CONF_SOURCE_ADDRESS, default=0x24): cv.hex_uint8_t,
    cv.Optional(CONF_COMMAND_ID, default=0x0F002420): cv.hex_uint32_t,
    cv.Optional(CONF_TEMP_TANK_ID, default=0x1BFD0224): cv.hex_uint32_t,
    cv.Optional(CONF_OUTPUT_STATUS_ID, default=0x1BFD0024): cv.hex_uint32_t,
    cv.Optional(CONF_CHANNEL_STATUS_ID, default=0x1BFCF024): cv.hex_uint32_t,
    cv.Optional(CONF_OUTPUTS): cv.ensure_list(OUTPUT_SCHEMA),
    cv.Optional(CONF_INVERTER): INVERTER_SCHEMA,
    cv.Optional(CONF_TEMP1): sensor.sensor_schema(),
    cv.Optional(CONF_TEMP2): sensor.sensor_schema(),
    cv.Optional(CONF_TANKS): cv.ensure_list(sensor.sensor_schema()),
    cv.Optional(CONF_LAST_COMMAND_CHANNEL): sensor.sensor_schema(),
    cv.Optional(CONF_LAST_COMMAND_STATE): sensor.sensor_schema(),
}).extend(cv.COMPONENT_SCHEMA)

async def _setup_sensor(var, config, key, setter):
    if key in config:
        sens = await sensor.new_sensor(config[key])
        cg.add(getattr(var, setter)(sens))

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    can = await cg.get_variable(config[CONF_CANBUS_ID])
    cg.add(var.set_canbus(can))
    cg.add(var.set_source_address(config[CONF_SOURCE_ADDRESS]))
    cg.add(var.set_command_id(config[CONF_COMMAND_ID]))
    cg.add(var.set_temp_tank_id(config[CONF_TEMP_TANK_ID]))
    cg.add(var.set_output_status_id(config[CONF_OUTPUT_STATUS_ID]))
    cg.add(var.set_channel_status_id(config[CONF_CHANNEL_STATUS_ID]))

    await _setup_sensor(var, config, CONF_TEMP1, "set_temp1_sensor")
    await _setup_sensor(var, config, CONF_TEMP2, "set_temp2_sensor")
    await _setup_sensor(var, config, CONF_LAST_COMMAND_CHANNEL, "set_last_command_channel_sensor")
    await _setup_sensor(var, config, CONF_LAST_COMMAND_STATE, "set_last_command_state_sensor")

    if CONF_TANKS in config:
        for idx, tank_conf in enumerate(config[CONF_TANKS], start=1):
            if idx > 6:
                break
            sens = await sensor.new_sensor(tank_conf)
            cg.add(var.set_tank_sensor(idx, sens))

    for out_conf in config.get(CONF_OUTPUTS, []):
        sw = await switch.new_switch(out_conf)
        output_number = out_conf[CONF_OUTPUT_NUMBER]
        channel = out_conf.get(CONF_CHANNEL, 0x03 + output_number)
        cg.add(sw.set_parent(var))
        cg.add(sw.set_output_number(output_number))
        cg.add(sw.set_channel(channel))
        cg.add(sw.set_is_inverter(False))
        cg.add(var.register_output_switch(sw))

    if CONF_INVERTER in config:
        inv_conf = config[CONF_INVERTER]
        sw = await switch.new_switch(inv_conf)
        cg.add(sw.set_parent(var))
        cg.add(sw.set_output_number(0))
        cg.add(sw.set_channel(inv_conf[CONF_CHANNEL]))
        cg.add(sw.set_is_inverter(True))
        cg.add(var.register_inverter_switch(sw))
