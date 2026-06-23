import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import canbus
from esphome.const import CONF_ID

CODEOWNERS = ["@dev8edss"]
DEPENDENCIES = ["canbus"]

CONF_CANBUS_ID = "canbus_id"
CONF_HOST_ADDRESS = "host_address"
CONF_DISCOVERY_DELAY = "discovery_delay"

ns = cg.esphome_ns.namespace("redarc_common")
RedarcCommonComponent = ns.class_("RedarcCommonComponent", cg.Component)

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(RedarcCommonComponent),
    cv.Required(CONF_CANBUS_ID): cv.use_id(canbus.CanbusComponent),
    cv.Optional(CONF_HOST_ADDRESS, default=0x22): cv.hex_uint8_t,
    cv.Optional(CONF_DISCOVERY_DELAY, default="1500ms"): cv.positive_time_period_milliseconds,
}).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    can = await cg.get_variable(config[CONF_CANBUS_ID])
    cg.add(var.set_canbus(can))
    cg.add(var.set_host_address(config[CONF_HOST_ADDRESS]))
    cg.add(var.set_discovery_delay_ms(config[CONF_DISCOVERY_DELAY]))
