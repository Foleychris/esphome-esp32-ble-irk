#include "esp32_ble_irk.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include <esp_bt_main.h>
#include <esp_gap_ble_api.h>
#include <esp_bt_device.h>
#include <cstdio>
#include <iomanip>
#include <sstream>

namespace esphome {
namespace esp32_ble_irk {

static const char *const TAG = "esp32_ble_irk";

// ESP32BLEIrk Implementation

void ESP32BLEIrk::setup() {
  ESP_LOGCONFIG(TAG, "Setting up ESP32 BLE IRK...");
  
  // Debug: Log the actual enum values
  ESP_LOGW(TAG, "ESP_GAP_BLE_PASSKEY_REQ_EVT = %d", ESP_GAP_BLE_PASSKEY_REQ_EVT);
  ESP_LOGW(TAG, "ESP_GAP_BLE_PASSKEY_NOTIF_EVT = %d", ESP_GAP_BLE_PASSKEY_NOTIF_EVT);
  ESP_LOGW(TAG, "ESP_GAP_BLE_NC_REQ_EVT = %d", ESP_GAP_BLE_NC_REQ_EVT);
  ESP_LOGW(TAG, "ESP_GAP_BLE_SEC_REQ_EVT = %d", ESP_GAP_BLE_SEC_REQ_EVT);
  
  // Configure BLE security parameters to force pairing/bonding
  ESP_LOGD(TAG, "Configuring BLE security parameters");
  
  // Set IO capability to NONE (critical for iOS pairing!)
  /*esp_ble_io_cap_t iocap = ESP_IO_CAP_NONE;
  esp_err_t ret = esp_ble_gap_set_security_param(ESP_BLE_SM_IOCAP_MODE, &iocap, sizeof(uint8_t));
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to set IO capability: %s", esp_err_to_name(ret));
  } */
  
  // Set authentication requirements - force bonding with MITM protection
  esp_ble_auth_req_t auth_req = ESP_LE_AUTH_REQ_SC_MITM_BOND;
  esp_err_t ret = esp_ble_gap_set_security_param(ESP_BLE_SM_AUTHEN_REQ_MODE, &auth_req, sizeof(uint8_t));
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to set auth req mode: %s", esp_err_to_name(ret));
  }
  
  // Set key size
  uint8_t key_size = 16;
  ret = esp_ble_gap_set_security_param(ESP_BLE_SM_MAX_KEY_SIZE, &key_size, sizeof(uint8_t));
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to set max key size: %s", esp_err_to_name(ret));
  }
  
  // Set key distribution - we want both encryption and identity keys
  uint8_t init_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
  uint8_t rsp_key = ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK;
  
  ret = esp_ble_gap_set_security_param(ESP_BLE_SM_SET_INIT_KEY, &init_key, sizeof(uint8_t));
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to set init key: %s", esp_err_to_name(ret));
  }
  
  ret = esp_ble_gap_set_security_param(ESP_BLE_SM_SET_RSP_KEY, &rsp_key, sizeof(uint8_t));
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to set rsp key: %s", esp_err_to_name(ret));
  }
  
  // Set static passkey (for devices that support it)
  uint32_t passkey = 123456;
  ret = esp_ble_gap_set_security_param(ESP_BLE_SM_SET_STATIC_PASSKEY, &passkey, sizeof(uint32_t));
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to set static passkey: %s", esp_err_to_name(ret));
  }
  
  // Only accept specified authentication (disable legacy pairing)
  uint8_t auth_option = ESP_BLE_ONLY_ACCEPT_SPECIFIED_AUTH_DISABLE;
  ret = esp_ble_gap_set_security_param(ESP_BLE_SM_ONLY_ACCEPT_SPECIFIED_SEC_AUTH, &auth_option, sizeof(uint8_t));
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to set auth option: %s", esp_err_to_name(ret));
  }
  
  ESP_LOGI(TAG, "BLE security parameters configured for forced bonding");
  
  if (this->clear_bonds_on_startup_) {
    ESP_LOGD(TAG, "Clearing all bonded devices on startup");
    this->clear_all_bonds();
  }
}

void ESP32BLEIrk::dump_config() {
  ESP_LOGCONFIG(TAG, "ESP32 BLE IRK:");
  ESP_LOGCONFIG(TAG, "  Auto extract: %s", YESNO(this->auto_extract_));
  ESP_LOGCONFIG(TAG, "  Clear bonds on startup: %s", YESNO(this->clear_bonds_on_startup_));
  ESP_LOGCONFIG(TAG, "  Text sensors: %d", this->text_sensors_.size());
}

float ESP32BLEIrk::get_setup_priority() const {
  return setup_priority::AFTER_BLUETOOTH;
}

void ESP32BLEIrk::gatts_event_handler(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t *param) {
  switch (event) {
    case ESP_GATTS_CONNECT_EVT: {
      ESP_LOGD(TAG, "GATTS connection from device, forcing encryption for pairing");
      
      // Emit pairing started event
      std::string address = this->format_address(param->connect.remote_bda);
      this->emit_(BLEIrkEvt::ON_PAIRING_STARTED, address, false, 0, 0);
      
      // Force encryption/pairing when a device connects
      esp_err_t ret = esp_ble_set_encryption(param->connect.remote_bda, ESP_BLE_SEC_ENCRYPT_MITM);
      if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to set encryption: %s", esp_err_to_name(ret));
      }
      break;
    }
    
    case ESP_GATTS_DISCONNECT_EVT: {
      ESP_LOGD(TAG, "GATTS disconnection, reason: 0x%02x", param->disconnect.reason);
      break;
    }
    
    default:
      // We don't need to handle other GATTS events
      break;
  }
}

void ESP32BLEIrk::gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param) {
  ESP_LOGD(TAG, "GAP event received: %d (0x%02x)", event, event);
  switch (event) {
    case ESP_GAP_BLE_SEC_REQ_EVT: {
      ESP_LOGD(TAG, "BLE security request received");
      // Accept the security request and start pairing
      esp_ble_gap_security_rsp(param->ble_security.ble_req.bd_addr, true);
      break;
    }
    
    case ESP_GAP_BLE_NC_REQ_EVT: {
      ESP_LOGD(TAG, "BLE numeric comparison request, passkey: %06d", param->ble_security.key_notif.passkey);
      // Auto-confirm for devices with no input capability
      esp_ble_confirm_reply(param->ble_security.ble_req.bd_addr, true);
      break;
    }
    
    case ESP_GAP_BLE_PASSKEY_NOTIF_EVT: {
      ESP_LOGI(TAG, "BLE passkey notification: %06d", param->ble_security.key_notif.passkey);
      break;
    }
    
    case ESP_GAP_BLE_PASSKEY_REQ_EVT: {
      ESP_LOGD(TAG, "BLE passkey request received");
      // Send our static passkey (123456) 
      esp_ble_passkey_reply(param->ble_security.ble_req.bd_addr, true, 123456);
      break;
    }
    
    case ESP_GAP_BLE_AUTH_CMPL_EVT: {
      ESP_LOGD(TAG, "BLE authentication complete");
      esp_bd_addr_t bd_addr;
      memcpy(bd_addr, param->ble_security.auth_cmpl.bd_addr, sizeof(esp_bd_addr_t));
      
      ESP_LOGD(TAG, "Remote BD_ADDR: %02X:%02X:%02X:%02X:%02X:%02X",
               bd_addr[0], bd_addr[1], bd_addr[2], bd_addr[3], bd_addr[4], bd_addr[5]);
      ESP_LOGD(TAG, "Address type = %d", param->ble_security.auth_cmpl.addr_type);
      ESP_LOGD(TAG, "Pair status = %s", param->ble_security.auth_cmpl.success ? "success" : "fail");
      
      // Emit pairing complete event
      std::string address = this->format_address(bd_addr);
      bool success = param->ble_security.auth_cmpl.success;
      uint8_t reason = success ? 0 : param->ble_security.auth_cmpl.fail_reason;
      this->emit_(BLEIrkEvt::ON_PAIRING_COMPLETE, address, success, reason, 0);
      
      if (!success) {
        ESP_LOGW(TAG, "Pairing failed, reason = 0x%x", param->ble_security.auth_cmpl.fail_reason);
      } else {
        ESP_LOGD(TAG, "Pairing successful");
        if (this->auto_extract_) {
          ESP_LOGD(TAG, "Auto-extracting IRK data after successful pairing");
          this->extract_irk_data();
        }
      }
      break;
    }
    
    case ESP_GAP_BLE_REMOVE_BOND_DEV_COMPLETE_EVT:
      ESP_LOGD(TAG, "Bond device removal complete, status = %d", param->remove_bond_dev_cmpl.status);
      // Update text sensors after bond removal
      this->extract_irk_data();
      break;
      
    default:
      // We don't need to handle other GAP events
      break;
  }
}

void ESP32BLEIrk::extract_irk_data() {
  ESP_LOGD(TAG, "Extracting IRK data from bonded devices");
  
  // Clear existing device info
  this->device_info_.clear();
  
  // Get number of bonded devices
  int dev_num = esp_ble_get_bond_device_num();
  ESP_LOGD(TAG, "Number of bonded devices: %d", dev_num);
  
  if (dev_num == 0) {
    ESP_LOGD(TAG, "No bonded devices found");
    this->update_text_sensors();
    return;
  }
  
  // Allocate memory for device list
  esp_ble_bond_dev_t *dev_list = (esp_ble_bond_dev_t *) malloc(sizeof(esp_ble_bond_dev_t) * dev_num);
  if (dev_list == nullptr) {
    ESP_LOGE(TAG, "Failed to allocate memory for device list");
    return;
  }
  
  // Get bonded device list
  esp_err_t ret = esp_ble_get_bond_device_list(&dev_num, dev_list);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to get bond device list: %s", esp_err_to_name(ret));
    free(dev_list);
    return;
  }
  
  ESP_LOGI(TAG, "Found %d bonded device(s)", dev_num);
  
  // Process each bonded device
  for (int i = 0; i < dev_num; i++) {
    DeviceIrkInfo device_info;
    
    // Copy device address
    memcpy(device_info.bd_addr, dev_list[i].bd_addr, sizeof(esp_bd_addr_t));
    
    // Copy IRK
    memcpy(device_info.irk, dev_list[i].bond_key.pid_key.irk, sizeof(esp_bt_octet16_t));
    
    // Format address and IRK strings
    device_info.address_str = this->format_address(device_info.bd_addr);
    device_info.irk_hex = this->format_irk_hex(device_info.irk);
    device_info.irk_base64 = this->format_irk_base64(device_info.irk);
    device_info.irk_reverse = this->format_irk_reverse(device_info.irk);
    
    // Log IRK information
    ESP_LOGI(TAG, "Device %d:", i + 1);
    ESP_LOGI(TAG, "  Address: %s", device_info.address_str.c_str());
    ESP_LOGI(TAG, "  IRK (hex): %s", device_info.irk_hex.c_str());
    ESP_LOGI(TAG, "  IRK (base64): %s", device_info.irk_base64.c_str());
    ESP_LOGI(TAG, "  IRK (reverse): %s", device_info.irk_reverse.c_str());
    
    this->device_info_.push_back(device_info);
  }
  
  // Clean up
  free(dev_list);
  
  // Update text sensors with new data
  this->update_text_sensors();
  
  // Emit IRK extracted event
  this->emit_(BLEIrkEvt::ON_IRK_EXTRACTED, "", false, 0, (uint16_t)dev_num);
}

void ESP32BLEIrk::clear_all_bonds() {
  ESP_LOGD(TAG, "Clearing all bonded devices");
  
  int dev_num = esp_ble_get_bond_device_num();
  if (dev_num == 0) {
    ESP_LOGD(TAG, "No bonded devices to clear");
    return;
  }
  
  esp_ble_bond_dev_t *dev_list = (esp_ble_bond_dev_t *) malloc(sizeof(esp_ble_bond_dev_t) * dev_num);
  if (dev_list == nullptr) {
    ESP_LOGE(TAG, "Failed to allocate memory for device list");
    return;
  }
  
  esp_err_t ret = esp_ble_get_bond_device_list(&dev_num, dev_list);
  if (ret != ESP_OK) {
    ESP_LOGE(TAG, "Failed to get bond device list: %s", esp_err_to_name(ret));
    free(dev_list);
    return;
  }
  
  ESP_LOGI(TAG, "Removing %d bonded device(s)", dev_num);
  
  for (int i = 0; i < dev_num; i++) {
    ret = esp_ble_remove_bond_device(dev_list[i].bd_addr);
    if (ret != ESP_OK) {
      ESP_LOGW(TAG, "Failed to remove bond device %d: %s", i, esp_err_to_name(ret));
    }
  }
  
  free(dev_list);
  
  // Clear our device info
  this->device_info_.clear();
  this->update_text_sensors();
  
  // Emit bond cleared event
  this->emit_(BLEIrkEvt::ON_BOND_CLEARED, "", false, 0, 0);
}

std::string ESP32BLEIrk::format_address(const esp_bd_addr_t &addr) {
  char addr_str[18];
  snprintf(addr_str, sizeof(addr_str), "%02X:%02X:%02X:%02X:%02X:%02X",
           addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);
  return std::string(addr_str);
}

std::string ESP32BLEIrk::format_irk_hex(const esp_bt_octet16_t &irk) {
  std::stringstream ss;
  for (int i = 0; i < 16; i++) {
    if (i > 0) ss << ",";
    ss << "0x" << std::hex << std::setfill('0') << std::setw(2) << (unsigned int) irk[i];
  }
  return ss.str();
}

std::string ESP32BLEIrk::format_irk_base64(const esp_bt_octet16_t &irk) {
  return base64_encode(irk, 16);
}

std::string ESP32BLEIrk::format_irk_reverse(const esp_bt_octet16_t &irk) {
  std::stringstream ss;
  for (int i = 15; i >= 0; i--) {
    ss << std::hex << std::setfill('0') << std::setw(2) << (unsigned int) irk[i];
  }
  return ss.str();
}

void ESP32BLEIrk::update_text_sensors() {
  ESP_LOGD(TAG, "Updating %d text sensor(s)", this->text_sensors_.size());
  
  for (auto *sensor : this->text_sensors_) {
    std::string value = "";
    
    if (!this->device_info_.empty()) {
      // For now, use the first device's data
      // TODO: Could be extended to support multiple devices
      const DeviceIrkInfo &device = this->device_info_[0];
      
      switch (sensor->get_format()) {
        case IRK_FORMAT_HEX:
          value = device.irk_hex;
          break;
        case IRK_FORMAT_BASE64:
          value = device.irk_base64;
          break;
        case IRK_FORMAT_REVERSE:
          value = device.irk_reverse;
          break;
        case IRK_FORMAT_ADDRESS:
          value = device.address_str;
          break;
      }
    }
    
    sensor->update_value(value);
  }
}

// ESP32BLEIrkTextSensor Implementation

void ESP32BLEIrkTextSensor::dump_config() {
  LOG_TEXT_SENSOR("", "ESP32 BLE IRK Text Sensor", this);
  const char *format_str = "";
  switch (this->format_) {
    case IRK_FORMAT_HEX:
      format_str = "hex";
      break;
    case IRK_FORMAT_BASE64:
      format_str = "base64";
      break;
    case IRK_FORMAT_REVERSE:
      format_str = "reverse";
      break;
    case IRK_FORMAT_ADDRESS:
      format_str = "address";
      break;
  }
  ESP_LOGCONFIG(TAG, "  Format: %s", format_str);
}

void ESP32BLEIrkTextSensor::update_value(const std::string &value) {
  if (this->state != value) {
    ESP_LOGD(TAG, "Updating text sensor '%s' with value: %s", this->get_name().c_str(), value.c_str());
    this->publish_state(value);
  }
}

}  // namespace esp32_ble_irk
}  // namespace esphome
