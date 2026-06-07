import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import canbus
from esphome.const import CONF_ID

CODEOWNERS = ["@dev8edss"]
DEPENDENCIES = ["canbus"]

CONF_CANBUS_ID = "canbus_id"

ns = cg.esphome_ns.namespace("redarc_common")
RedarcCommonComponent = ns.class_("RedarcCommonComponent", cg.Component)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(RedarcCommonComponent),
    cv.Required(CONF_CANBUS_ID): cv.use_id(canbus.CanbusComponent),
}).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    can = await cg.get_variable(config[CONF_CANBUS_ID])
    cg.add(var.set_canbus(can))
