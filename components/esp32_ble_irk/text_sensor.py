import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import text_sensor
from esphome.const import CONF_ID, CONF_NAME

from . import ESP32BLEIrk, esp32_ble_irk_ns, IRK_FORMATS, CONF_BLE_IRK_ID

CONF_FORMAT = "format"

ESP32BLEIrkTextSensor = esp32_ble_irk_ns.class_(
    "ESP32BLEIrkTextSensor", text_sensor.TextSensor, cg.Component
)

CONFIG_SCHEMA = text_sensor.text_sensor_schema().extend(
    {
        cv.GenerateID(): cv.declare_id(ESP32BLEIrkTextSensor),
        cv.GenerateID(CONF_BLE_IRK_ID): cv.use_id(ESP32BLEIrk),
        cv.Required(CONF_FORMAT): cv.enum(IRK_FORMATS, lower=True),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await text_sensor.register_text_sensor(var, config)
    await cg.register_component(var, config)

    parent = await cg.get_variable(config[CONF_BLE_IRK_ID])
    cg.add(var.set_parent(parent))
    cg.add(var.set_format(config[CONF_FORMAT]))
    cg.add(parent.add_text_sensor(var))
