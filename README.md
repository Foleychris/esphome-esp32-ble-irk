# ESP32 BLE IRK Component for ESPHome

Extract Identity Resolving Keys (IRK) from bonded Bluetooth Low Energy devices on ESP32 for Home Assistant integrations and device tracking.

## 🏠 About This Project

This component was created by a hobbyist developer with significant assistance from Claude AI. While it's been tested and works well, there might be coding oddities or improvements that could be made. **Pull requests are very welcome!** Whether it's code cleanup, better error handling, documentation improvements, or new features - I'm happy to review and merge contributions.

## ✨ What It Does

- Extracts IRK (Identity Resolving Key) from BLE devices that pair with your ESP32
- Provides multiple output formats for different use cases
- Integrates seamlessly with ESPHome and Home Assistant
- Handles device pairing events and bond management

## 🚀 Quick Start

### 1. Add to your ESPHome configuration:

```yaml
external_components:
  - source: github://Foleychris/esphome-esp32-ble-irk
    components: [ esp32_ble_irk ]

esp32_ble:
esp32_ble_server:  # Optional - for devices to connect to

esp32_ble_irk:
  auto_extract: true

text_sensor:
  - platform: esp32_ble_irk
    format: base64
    name: "IRK (Base64)"
```

### 2. Pair a BLE device with your ESP32
- The IRK will automatically be extracted and displayed in your text sensor

## 📋 Output Formats

- **`base64`** - For Home Assistant Private BLE Device integration
- **`hex`** - Human-readable hex format for debugging  
- **`reverse`** - For ESPresense room presence detection
- **`address`** - Device MAC address

## 🔧 Configuration Options

```yaml
esp32_ble_irk:
  auto_extract: true              # Extract IRK after pairing (default: true)
  clear_bonds_on_startup: false   # Clear bonds on boot (default: false)
  
  # Optional automation triggers
  on_pairing_complete:
    - logger.log: "Device paired successfully!"
  on_irk_extracted:
    - homeassistant.event:
        event: irk_extracted
        data:
          irk: !lambda 'return id(my_irk_sensor).state;'
```

## 🎯 Use Cases

### Home Assistant Private BLE Device
```yaml
# In Home Assistant configuration.yaml
private_ble_device:
  - name: "My Private Device"
    irk: "BASE64_IRK_FROM_SENSOR"
```

### ESPresense Room Presence
```yaml
# In ESPresense configuration
known_irks:
  - id: "my_device"
    irk: "REVERSE_HEX_IRK_FROM_SENSOR"
```

## 📝 Full Example

See [`example.yaml`](example.yaml) for a complete configuration showing all features including:
- BLE server setup for device pairing
- Multiple text sensor formats
- Control buttons for manual operations  
- Home Assistant event integration
- Automation triggers

## ⚠️ Requirements

- **ESP32** (not ESP32-S2 - lacks BLE support)
- **ESPHome** with BLE support
- **Device with BLE Privacy** - not all devices use IRK

## 🐛 Troubleshooting

**No IRK extracted?**
- Check that your device supports BLE Privacy/Random addresses
- Verify successful pairing in the logs
- Some devices don't use IRK - this is normal

**Pairing issues?**
- Try different `io_capability` settings in `esp32_ble`
- Clear existing bonds if having connection problems
- Check device-specific pairing requirements

## 🤝 Contributing

This project benefits from community input! Areas where help is especially welcome:

- **Code Review** - Spot any quirks from the hobbyist + AI development approach
- **Error Handling** - More robust edge case handling
- **Documentation** - Clearer examples or troubleshooting guides  
- **Testing** - Validation with different device types
- **Features** - Additional output formats or automation capabilities

Feel free to open issues or submit pull requests. I'm learning too, so constructive feedback is appreciated!

## 📄 License

This project is open source. Feel free to use, modify, and distribute.

## 🙏 Acknowledgments

- Based on original ESP32 IRK extraction concepts
- Developed with assistance from Claude AI
- Thanks to the ESPHome community for the framework
