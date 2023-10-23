#pragma once

#include "esphome/components/climate_ir/climate_ir.h"

#include <cinttypes>

namespace esphome {
namespace climate_ir_sinclair {

// Temperature
const uint8_t TEMP_MIN = 16;  // Celsius
const uint8_t TEMP_MAX = 30;  // Celsius

class SinclairIrClimate : public climate_ir::ClimateIR {
 public:
  SinclairIrClimate()
      : climate_ir::ClimateIR(TEMP_MIN, TEMP_MAX, 1.0f, true, true,
                              {climate::CLIMATE_FAN_AUTO, climate::CLIMATE_FAN_LOW, climate::CLIMATE_FAN_MEDIUM,
                               climate::CLIMATE_FAN_MIDDLE, climate::CLIMATE_FAN_HIGH},
                              {climate::CLIMATE_SWING_OFF}) {}
  void set_header_high(uint32_t header_high) { this->header_high_ = header_high; }
  void set_header_low(uint32_t header_low) { this->header_low_ = header_low; }
  void set_bit_high(uint32_t bit_high) { this->bit_high_ = bit_high; }
  void set_bit_one_low(uint32_t bit_one_low) { this->bit_one_low_ = bit_one_low; }
  void set_bit_zero_low(uint32_t bit_zero_low) { this->bit_zero_low_ = bit_zero_low; }

 protected:
  /// Transmit via IR the state of this climate controller.
  void transmit_state() override;
  /// Handle received IR Buffer
  bool on_receive(remote_base::RemoteReceiveData data) override;

  void transmit_(uint64_t value);

  uint32_t mode_before_{0};

  uint32_t header_high_;
  uint32_t header_low_;
  uint32_t bit_high_;
  uint32_t bit_one_low_;
  uint32_t bit_zero_low_;
};

}  // namespace climate_ir_sinclair
}  // namespace esphome
