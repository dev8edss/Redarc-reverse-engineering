import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import light
from esphome.const import CONF_OUTPUT_ID
from . import tvms_rogue_ns, TVMSRogueComponent

CONF_TVMS_ROGUE_ID = "tvms_rogue_id"
CONF_OUTPUT_NUMBER = "output_number"
CONF_CHANNEL = "channel"

TVMSRogueLight = tvms_rogue_ns.class_("TVMSRogueLight", light.LightOutput)

CONFIG_SCHEMA = light.BRIGHTNESS_ONLY_LIGHT_SCHEMA.extend(
    {
        cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(TVMSRogueLight),
        cv.Required(CONF_TVMS_ROGUE_ID): cv.use_id(TVMSRogueComponent),
        cv.Required(CONF_OUTPUT_NUMBER): cv.int_range(min=1, max=10),
        cv.Optional(CONF_CHANNEL): cv.hex_uint8_t,
    }
)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    await light.register_light(var, config)
    hub = await cg.get_variable(config[CONF_TVMS_ROGUE_ID])
    output_number = config[CONF_OUTPUT_NUMBER]
    channel = config.get(CONF_CHANNEL, 0x0B + output_number)
    cg.add(var.set_parent(hub))
    cg.add(var.set_output_number(output_number))
    cg.add(var.set_channel(channel))
    cg.add(hub.register_light(var))
