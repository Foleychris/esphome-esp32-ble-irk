#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/components/esp32_ble/ble_event.h"
#include "esphome/components/esp32_ble/ble.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/event_emitter/event_emitter.h"

#include <esp_gap_ble_api.h>
#include <esp_bt_defs.h>
#include <esp_gatts_api.h>
#include <vector>
#include <string>

namespace esphome {
namespace esp32_ble_irk {

using namespace esp32_ble;
using namespace event_emitter;

// Forward declarations
class ESP32BLEIrkTextSensor;

namespace BLEIrkEvt {
enum IrkEvent {
  ON_PAIRING_STARTED,
  ON_PAIRING_COMPLETE,
  ON_IRK_EXTRACTED,
  ON_BOND_CLEARED,
};
}  // namespace BLEIrkEvt

enum IrkFormat {
  IRK_FORMAT_HEX,
  IRK_FORMAT_BASE64,
  IRK_FORMAT_REVERSE,
  IRK_FORMAT_ADDRESS,
};

struct DeviceIrkInfo {
  esp_bd_addr_t bd_addr;
  esp_bt_octet16_t irk;
  std::string address_str;
  std::string irk_hex;
  std::string irk_base64;
  std::string irk_reverse;
};

class ESP32BLEIrk : public Component, 
                    public GAPEventHandler, 
                    public GATTsEventHandler, 
                    public Parented<ESP32BLE>,
                    public EventEmitter<BLEIrkEvt::IrkEvent, std::string, bool, uint8_t, uint16_t> {
 public:
  void setup() override;
  void dump_config() override;
  void loop() override;
  float get_setup_priority() const override;

  // GAP Event Handler
  void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) override;
  
  // GATTS Event Handler  
  void gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param) override;

  // Configuration setters
  void set_auto_extract(bool auto_extract) { this->auto_extract_ = auto_extract; }
  void set_clear_bonds_on_startup(bool clear_bonds) { this->clear_bonds_on_startup_ = clear_bonds; }

  // Text sensor management
  void add_text_sensor(ESP32BLEIrkTextSensor *sensor) { this->text_sensors_.push_back(sensor); }

  // IRK operations
  void extract_irk_data();
  void clear_all_bonds();
  
  // Get the latest IRK information
  const std::vector<DeviceIrkInfo> &get_device_info() const { return this->device_info_; }

 protected:
  bool auto_extract_{true};
  bool clear_bonds_on_startup_{false};
  std::vector<ESP32BLEIrkTextSensor *> text_sensors_;
  std::vector<DeviceIrkInfo> device_info_;

  // Helper methods
  std::string format_address(const esp_bd_addr_t &addr);
  std::string format_irk_hex(const esp_bt_octet16_t &irk);
  std::string format_irk_base64(const esp_bt_octet16_t &irk);
  std::string format_irk_reverse(const esp_bt_octet16_t &irk);
  void update_text_sensors();
};

class ESP32BLEIrkTextSensor : public text_sensor::TextSensor, public Component {
 public:
  void set_parent(ESP32BLEIrk *parent) { this->parent_ = parent; }
  void set_format(IrkFormat format) { this->format_ = format; }
  IrkFormat get_format() const { return this->format_; }
  void dump_config() override;
  void update_value(const std::string &value);

 protected:
  ESP32BLEIrk *parent_;
  IrkFormat format_;
};

template<typename... Ts> class ESP32BLEIrkExtractAction : public Action<Ts...> {
 public:
  explicit ESP32BLEIrkExtractAction(ESP32BLEIrk *parent) : parent_(parent) {}

  void play(Ts... x) override { this->parent_->extract_irk_data(); }

 protected:
  ESP32BLEIrk *parent_;
};

template<typename... Ts> class ESP32BLEIrkClearBondsAction : public Action<Ts...> {
 public:
  explicit ESP32BLEIrkClearBondsAction(ESP32BLEIrk *parent) : parent_(parent) {}

  void play(Ts... x) override { this->parent_->clear_all_bonds(); }

 protected:
  ESP32BLEIrk *parent_;
};

// Automation triggers
class ESP32BLEIrkPairingStartedTrigger : public Trigger<std::string> {
 public:
  explicit ESP32BLEIrkPairingStartedTrigger(ESP32BLEIrk *parent) {
    parent->on(BLEIrkEvt::ON_PAIRING_STARTED, [this](std::string address, bool, uint8_t, uint16_t) {
      this->trigger(address);
    });
  }
};

class ESP32BLEIrkPairingCompleteTrigger : public Trigger<std::string, bool, uint8_t> {
 public:
  explicit ESP32BLEIrkPairingCompleteTrigger(ESP32BLEIrk *parent) {
    parent->on(BLEIrkEvt::ON_PAIRING_COMPLETE, [this](std::string address, bool success, uint8_t reason, uint16_t) {
      this->trigger(address, success, reason);
    });
  }
};

class ESP32BLEIrkExtractedTrigger : public Trigger<uint16_t> {
 public:
  explicit ESP32BLEIrkExtractedTrigger(ESP32BLEIrk *parent) {
    parent->on(BLEIrkEvt::ON_IRK_EXTRACTED, [this](std::string, bool, uint8_t, uint16_t device_count) {
      this->trigger(device_count);
    });
  }
};

class ESP32BLEIrkBondClearedTrigger : public Trigger<> {
 public:
  explicit ESP32BLEIrkBondClearedTrigger(ESP32BLEIrk *parent) {
    parent->on(BLEIrkEvt::ON_BOND_CLEARED, [this](std::string, bool, uint8_t, uint16_t) {
      this->trigger();
    });
  }
};

}  // namespace esp32_ble_irk
}  // namespace esphome
