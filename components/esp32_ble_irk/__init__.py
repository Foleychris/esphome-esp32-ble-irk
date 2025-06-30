import encodings
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.components import esp32_ble, text_sensor
from esphome.const import (
    CONF_ID,
    CONF_NAME,
    CONF_UPDATE_INTERVAL,
    DEVICE_CLASS_EMPTY,
    STATE_CLASS_NONE,
)
from esphome.core import CORE

AUTO_LOAD = ["text_sensor", "event_emitter"]
DEPENDENCIES = ["esp32_ble"]
CODEOWNERS = ["@yourusername"]

CONF_BLE_IRK_ID = "ble_irk_id"
CONF_AUTO_EXTRACT = "auto_extract"
CONF_CLEAR_BONDS_ON_STARTUP = "clear_bonds_on_startup"
CONF_IRK_HEX = "irk_hex"
CONF_IRK_BASE64 = "irk_base64" 
CONF_IRK_REVERSE = "irk_reverse"
CONF_DEVICE_ADDRESS = "device_address"
CONF_ON_PAIRING_STARTED = "on_pairing_started"
CONF_ON_PAIRING_COMPLETE = "on_pairing_complete"
CONF_ON_IRK_EXTRACTED = "on_irk_extracted"
CONF_ON_BOND_CLEARED = "on_bond_cleared"

esp32_ble_irk_ns = cg.esphome_ns.namespace("esp32_ble_irk")
ESP32BLEIrk = esp32_ble_irk_ns.class_(
    "ESP32BLEIrk", 
    cg.Component, 
    esp32_ble.GAPEventHandler,
    esp32_ble.GATTsEventHandler,
    cg.Parented.template(esp32_ble.ESP32BLE)
)

# Text sensor platform
ESP32BLEIrkTextSensor = esp32_ble_irk_ns.class_(
    "ESP32BLEIrkTextSensor",
    text_sensor.TextSensor,
    cg.Component,
)

# Actions
ESP32BLEIrkExtractAction = esp32_ble_irk_ns.class_(
    "ESP32BLEIrkExtractAction", automation.Action
)
ESP32BLEIrkClearBondsAction = esp32_ble_irk_ns.class_(
    "ESP32BLEIrkClearBondsAction", automation.Action
)

# Automation triggers
ESP32BLEIrkPairingStartedTrigger = esp32_ble_irk_ns.class_(
    "ESP32BLEIrkPairingStartedTrigger", automation.Trigger.template(cg.std_string)
)
ESP32BLEIrkPairingCompleteTrigger = esp32_ble_irk_ns.class_(
    "ESP32BLEIrkPairingCompleteTrigger", automation.Trigger.template(cg.std_string, cg.bool_, cg.uint8)
)
ESP32BLEIrkExtractedTrigger = esp32_ble_irk_ns.class_(
    "ESP32BLEIrkExtractedTrigger", automation.Trigger.template(cg.uint16)
)
ESP32BLEIrkBondClearedTrigger = esp32_ble_irk_ns.class_(
    "ESP32BLEIrkBondClearedTrigger", automation.Trigger.template()
)

# IRK Format enum
IrkFormat = esp32_ble_irk_ns.enum("IrkFormat")
IRK_FORMATS = {
    "hex": IrkFormat.IRK_FORMAT_HEX,
    "base64": IrkFormat.IRK_FORMAT_BASE64,
    "reverse": IrkFormat.IRK_FORMAT_REVERSE,
    "address": IrkFormat.IRK_FORMAT_ADDRESS,
}

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(ESP32BLEIrk),
        cv.GenerateID(esp32_ble.CONF_BLE_ID): cv.use_id(esp32_ble.ESP32BLE),
        cv.Optional(CONF_AUTO_EXTRACT, default=True): cv.boolean,
        cv.Optional(CONF_CLEAR_BONDS_ON_STARTUP, default=False): cv.boolean,
        cv.Optional(CONF_ON_PAIRING_STARTED): automation.validate_automation(single=True),
        cv.Optional(CONF_ON_PAIRING_COMPLETE): automation.validate_automation(single=True),
        cv.Optional(CONF_ON_IRK_EXTRACTED): automation.validate_automation(single=True),
        cv.Optional(CONF_ON_BOND_CLEARED): automation.validate_automation(single=True),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    # Register BT loggers this component needs
    esp32_ble.register_bt_logger(
        esp32_ble.BTLoggers.GAP,
        esp32_ble.BTLoggers.SMP,
        esp32_ble.BTLoggers.BTM,
    )
    
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    
    # Get parent ESP32BLE component
    parent = await cg.get_variable(config[esp32_ble.CONF_BLE_ID])
    cg.add(var.set_parent(parent))
    
    # Register as GAP event handler
    cg.add(parent.register_gap_event_handler(var))
    
    # Register as GATTS event handler
    cg.add(parent.register_gatts_event_handler(var))
    
    # Set configuration options
    cg.add(var.set_auto_extract(config[CONF_AUTO_EXTRACT]))
    cg.add(var.set_clear_bonds_on_startup(config[CONF_CLEAR_BONDS_ON_STARTUP]))
    
    # Register automation triggers
    if CONF_ON_PAIRING_STARTED in config:
        trigger = cg.new_Pvariable(ESP32BLEIrkPairingStartedTrigger, var)
        await automation.build_automation(trigger, [(cg.std_string, "address")], config[CONF_ON_PAIRING_STARTED])
    
    if CONF_ON_PAIRING_COMPLETE in config:
        trigger = cg.new_Pvariable(ESP32BLEIrkPairingCompleteTrigger, var)
        await automation.build_automation(trigger, [(cg.std_string, "address"), (cg.bool_, "success"), (cg.uint8, "reason")], config[CONF_ON_PAIRING_COMPLETE])
    
    if CONF_ON_IRK_EXTRACTED in config:
        trigger = cg.new_Pvariable(ESP32BLEIrkExtractedTrigger, var)
        await automation.build_automation(trigger, [(cg.uint16, "device_count")], config[CONF_ON_IRK_EXTRACTED])
    
    if CONF_ON_BOND_CLEARED in config:
        trigger = cg.new_Pvariable(ESP32BLEIrkBondClearedTrigger, var)
        await automation.build_automation(trigger, [], config[CONF_ON_BOND_CLEARED])
    
    cg.add_define("USE_ESP32_BLE_IRK")


@automation.register_action(
    "esp32_ble_irk.extract",
    ESP32BLEIrkExtractAction,
    cv.Schema({cv.Required(CONF_ID): cv.use_id(ESP32BLEIrk)}),
)
async def esp32_ble_irk_extract_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    return var

@automation.register_action(
    "esp32_ble_irk.clear_bonds",
    ESP32BLEIrkClearBondsAction,
    cv.Schema({cv.Required(CONF_ID): cv.use_id(ESP32BLEIrk)}),
)
async def esp32_ble_irk_clear_bonds_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    return var
