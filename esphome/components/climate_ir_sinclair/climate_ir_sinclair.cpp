#include "climate_ir_sinclair.h"
#include "esphome/core/log.h"

namespace esphome {
namespace climate_ir_sinclair {

static const char *const TAG = "climate.climate_ir_sinclair";

const uint32_t COMMAND_MASK = 0x0008;
const uint32_t COMMAND_OFF = 0x0000;
const uint32_t COMMAND_ON = 0x0008;

const uint32_t MODE_MASK = 0x0007;
const uint32_t MODE_AUTO = 0x0;
const uint32_t MODE_COOL = 0x1;
const uint32_t MODE_DRY = 0x2;
const uint32_t MODE_FAN = 0x3;
const uint32_t MODE_HEAT = 0x4;

const uint32_t FAN_MASK = 0x00F0;
const uint32_t FAN_AUTO = 0x0080;
const uint32_t FAN_LVL1 = 0x0010;
const uint32_t FAN_LVL2 = 0x0020;
const uint32_t FAN_LVL3 = 0x0030;
const uint32_t FAN_LVL4 = 0x0040;

// Temperature
const uint32_t TEMP_MASK = 0X0F00;
const uint32_t TEMP_SHIFT = 8;

const uint16_t BITS = 43;

void SinclairIrClimate::transmit_state() {
  uint64_t remote_state = 0x25150000000ULL;

  switch (this->mode) {
    case climate::CLIMATE_MODE_COOL:
      remote_state |= COMMAND_ON | MODE_COOL;
      break;
    case climate::CLIMATE_MODE_HEAT:
      remote_state |= COMMAND_ON | MODE_HEAT;
      break;
    case climate::CLIMATE_MODE_HEAT_COOL:
      remote_state |= COMMAND_ON | MODE_AUTO;
      break;
    case climate::CLIMATE_MODE_DRY:
      remote_state |= COMMAND_ON | MODE_DRY;
      break;
    case climate::CLIMATE_MODE_FAN_ONLY:
      remote_state |= COMMAND_ON | MODE_FAN;
      break;
    case climate::CLIMATE_MODE_OFF:
    default:
      remote_state |= COMMAND_OFF | mode_before_;
      break;
  }

  mode_before_ = remote_state & MODE_MASK;

  uint32_t temperature = (uint8_t) roundf(clamp<float>(this->target_temperature, TEMP_MIN, TEMP_MAX));
  remote_state |= ((temperature - TEMP_MIN) << TEMP_SHIFT);

  switch (this->fan_mode.value()) {
    case climate::CLIMATE_FAN_HIGH:
      remote_state |= FAN_LVL4;
      break;
    case climate::CLIMATE_FAN_MIDDLE:
      remote_state |= FAN_LVL3;
      break;
    case climate::CLIMATE_FAN_MEDIUM:
      remote_state |= FAN_LVL2;
      break;
    case climate::CLIMATE_FAN_LOW:
      remote_state |= FAN_LVL1;
      break;
    case climate::CLIMATE_FAN_AUTO:
    default:
      remote_state |= FAN_AUTO;
      break;
  }

  ESP_LOGD(TAG, "climate_sinclair_ir mode code: 0x%02X", this->mode);

  transmit_(remote_state);
  this->publish_state();
}

bool SinclairIrClimate::on_receive(remote_base::RemoteReceiveData data) {
  uint8_t nbits = 0;
  uint64_t remote_state = 0;

  if (!data.expect_item(this->header_high_, this->header_low_)) {
    return false;
  }

  for (nbits = 0; nbits < BITS; nbits++) {
    if (data.expect_item(this->bit_high_, this->bit_one_low_)) {
      remote_state = (remote_state >> 1) | (1ULL << (BITS-1));
    } else if (data.expect_item(this->bit_high_, this->bit_zero_low_)) {
      remote_state = (remote_state >> 1) | (0ULL << (BITS-1));
    } else {
      break;
    }
  }

  if (nbits != BITS) {
    ESP_LOGW(TAG, "Received %" PRIu8 " bits, expected %" PRIu8, nbits, BITS);
    return false;
  }

  ESP_LOGD(TAG, "Received code 0x%012" PRIX64, remote_state);

  if ((remote_state & 0xFFFF0000000ULL) != 0x25150000000ULL) {
    return false;
  }

  if ((remote_state & COMMAND_MASK) == COMMAND_OFF) {
    this->mode = climate::CLIMATE_MODE_OFF;
  } else if ((remote_state & COMMAND_MASK) == COMMAND_ON) {
    switch (remote_state & MODE_MASK) {
      case MODE_COOL:
        this->mode = climate::CLIMATE_MODE_COOL;
        break;
      case MODE_HEAT:
        this->mode = climate::CLIMATE_MODE_HEAT;
        break;
      case MODE_AUTO:
        this->mode = climate::CLIMATE_MODE_HEAT_COOL;
        break;
      case MODE_DRY:
        this->mode = climate::CLIMATE_MODE_DRY;
        break;
      case MODE_FAN:
        this->mode = climate::CLIMATE_MODE_FAN_ONLY;
        break;
      default:
        this->mode = climate::CLIMATE_MODE_OFF;
        break;
    }
  }

  // Temperature
  this->target_temperature = ((remote_state & TEMP_MASK) >> TEMP_SHIFT) + TEMP_MIN;

  // Fan Speed
  switch(remote_state & FAN_MASK) {
    case FAN_LVL4:
      this->fan_mode = climate::CLIMATE_FAN_HIGH;
      break;
    case FAN_LVL3:
      this->fan_mode = climate::CLIMATE_FAN_MIDDLE;
      break;
    case FAN_LVL2:
      this->fan_mode = climate::CLIMATE_FAN_MEDIUM;
      break;
    case FAN_LVL1:
      this->fan_mode = climate::CLIMATE_FAN_LOW;
      break;
    case FAN_AUTO:
    default:
      this->fan_mode = climate::CLIMATE_FAN_AUTO;
      break;
  }

  this->publish_state();
  return true;
}

void SinclairIrClimate::transmit_(uint64_t value) {
  ESP_LOGD(TAG, "Sending climate_sinclair_ir code: 0x%012" PRIX64, value);

  auto transmit = this->transmitter_->transmit();
  auto *data = transmit.get_data();

  data->set_carrier_frequency(38000);
  data->reserve(2 + BITS * 2u);

  data->item(this->header_high_, this->header_low_);

  // for (uint64_t mask = 1ULL << (BITS - 1); mask != 0; mask >>= 1) {
  for(uint64_t mask = 1; mask < (1ULL << (BITS)); mask <<= 1) {
    if (value & mask) {
      data->item(this->bit_high_, this->bit_one_low_);
    } else {
      data->item(this->bit_high_, this->bit_zero_low_);
    }
  }
  data->mark(this->bit_high_);
  transmit.perform();
}

}  // namespace climate_ir_sinclair
}  // namespace esphome
