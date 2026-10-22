import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import light

virtual_addressable_light_ns = cg.esphome_ns.namespace('virtual_addressable_light')
VirtualAddressableLight = virtual_addressable_light_ns.class_('VirtualAddressableLight', light.AddressableLight)

CONF_NUM_LEDS = 'num_leds'

CONFIG_SCHEMA = light.ADDRESSABLE_LIGHT_SCHEMA.extend({
    cv.GenerateID(light.CONF_OUTPUT_ID): cv.declare_id(VirtualAddressableLight),
    cv.Required(CONF_NUM_LEDS): cv.positive_not_null_int,
}).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[light.CONF_OUTPUT_ID])
    await light.register_light(var, config)
    await cg.register_component(var, config)
    cg.add(var.set_num_leds(config[CONF_NUM_LEDS]))
