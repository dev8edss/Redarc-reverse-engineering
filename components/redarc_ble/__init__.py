import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_DEVICE_NAME

DEPENDENCIES = ["esp32"]
AUTO_LOAD = ["esp32_ble"]

redarc_ble_ns = cg.esphome_ns.namespace("redarc_ble")
RedarcBLEComponent = redarc_ble_ns.class_("RedarcBLEComponent", cg.Component)

CONF_UPDATE_INTERVAL = "update_interval_ms"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(RedarcBLEComponent),
        cv.Optional(CONF_DEVICE_NAME, default="Redarc Bridge"): cv.string,
        cv.Optional(CONF_UPDATE_INTERVAL, default=1000): cv.positive_int,
    }
).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_device_name(config[CONF_DEVICE_NAME]))
    cg.add(var.set_update_interval_ms(config[CONF_UPDATE_INTERVAL]))
