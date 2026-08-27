#include "m5pm1.h"

#ifdef USE_ESP32

#include "esphome/core/log.h"

namespace esphome {
namespace m5pm1 {

static const char *const TAG = "m5pm1";

bool M5PM1Component::update_bits_(uint8_t reg, uint8_t mask, uint8_t value) {
  uint8_t current = 0;
  if (!this->read_byte(reg, &current))
    return false;
  const uint8_t updated = (current & ~mask) | (value & mask);
  if (updated == current)
    return true;
  return this->write_byte(reg, updated);
}

bool M5PM1Component::write_pin_(uint8_t pin, bool level) {
  const uint8_t bit = 1 << pin;
  return this->update_bits_(M5PM1_REG_GPIO_OUT, bit, level ? bit : 0);
}

// The register comments describe the high byte as "high 4 bits", but
// readVbat() in M5PM1.h returns a uint16_t of millivolts and a charged LiPo at
// 4200mV does not fit in 12 bits. Read the pair as plain little-endian uint16,
// unmasked.
bool M5PM1Component::read_mv_(uint8_t reg_low, uint16_t *mv) {
  uint8_t buf[2];
  if (!this->read_bytes(reg_low, buf, 2))
    return false;
  *mv = (uint16_t) buf[0] | ((uint16_t) buf[1] << 8);
  return true;
}

// Single-cell LiPo open-circuit curve. The M5PM1 exposes voltages only - no
// coulomb counter, no state-of-charge register - so this is an estimate. It
// reads high while charging and sags during WiFi transmit bursts, which on a
// 250mAh cell is not a small effect.
float M5PM1Component::battery_percent_(uint16_t mv) {
  struct Point {
    uint16_t mv;
    float pct;
  };
  static const Point CURVE[] = {
      {4200, 100.0f}, {4100, 94.0f}, {4000, 85.0f}, {3900, 76.0f},
      {3800, 62.0f},  {3750, 53.0f}, {3700, 44.0f}, {3650, 33.0f},
      {3600, 23.0f},  {3500, 12.0f}, {3400, 5.0f},  {3300, 0.0f},
  };
  const size_t n = sizeof(CURVE) / sizeof(CURVE[0]);

  if (mv >= CURVE[0].mv)
    return 100.0f;
  if (mv <= CURVE[n - 1].mv)
    return 0.0f;

  for (size_t i = 1; i < n; i++) {
    if (mv >= CURVE[i].mv) {
      const float span = (float) (CURVE[i - 1].mv - CURVE[i].mv);
      const float frac = (float) (mv - CURVE[i].mv) / span;
      return CURVE[i].pct + frac * (CURVE[i - 1].pct - CURVE[i].pct);
    }
  }
  return 0.0f;
}

void M5PM1Component::setup() {
  const uint8_t lcd_bit = 1 << M5PM1_PIN_LCD_RAIL;
  const uint8_t amp_bit = 1 << M5PM1_PIN_SPEAKER_AMP;
  const uint8_t both = lcd_bit | amp_bit;

  // Read-modify-write everywhere so the PYG pins we are not responsible for
  // keep whatever the PMIC or a previous boot left them at.

  // GPIO_FUNC0 packs two bits per pin: GPIO2 at [5:4], GPIO3 at [7:6].
  // 00 = plain GPIO rather than IRQ/WAKE/PWM.
  if (!this->update_bits_(M5PM1_REG_GPIO_FUNC0, 0xF0, 0x00)) {
    ESP_LOGE(TAG, "No response at 0x%02X - LCD rail will stay off", this->address_);
    this->mark_failed();
    return;
  }

  this->update_bits_(M5PM1_REG_GPIO_DRV, both, 0x00);   // push-pull
  this->update_bits_(M5PM1_REG_GPIO_MODE, both, both);  // outputs

  this->write_pin_(M5PM1_PIN_LCD_RAIL, this->lcd_power_);
  this->write_pin_(M5PM1_PIN_SPEAKER_AMP, this->speaker_amp_);

  ESP_LOGI(TAG, "L3B rail %s (PYG2), speaker amp %s (PYG3)",
           this->lcd_power_ ? "ON" : "off", this->speaker_amp_ ? "ON" : "off");
}

void M5PM1Component::update() {
#ifdef USE_SENSOR
  uint16_t mv = 0;

  if (this->battery_voltage_sensor_ != nullptr || this->battery_level_sensor_ != nullptr) {
    if (this->read_mv_(M5PM1_REG_VBAT_L, &mv)) {
      if (this->battery_voltage_sensor_ != nullptr)
        this->battery_voltage_sensor_->publish_state(mv / 1000.0f);
      if (this->battery_level_sensor_ != nullptr)
        this->battery_level_sensor_->publish_state(battery_percent_(mv));
    } else {
      ESP_LOGW(TAG, "VBAT read failed");
    }
  }

  if (this->input_voltage_sensor_ != nullptr) {
    if (this->read_mv_(M5PM1_REG_VIN_L, &mv))
      this->input_voltage_sensor_->publish_state(mv / 1000.0f);
  }
#endif
}

void M5PM1Component::dump_config() {
  ESP_LOGCONFIG(TAG, "M5PM1 PMIC:");
  LOG_I2C_DEVICE(this);
  ESP_LOGCONFIG(TAG, "  LCD rail (L3B/PYG2): %s", ONOFF(this->lcd_power_));
  ESP_LOGCONFIG(TAG, "  Speaker amp (PYG3): %s", ONOFF(this->speaker_amp_));
  if (this->is_failed())
    ESP_LOGE(TAG, "  Communication failed - the panel will be dark");
#ifdef USE_SENSOR
  LOG_SENSOR("  ", "Battery voltage", this->battery_voltage_sensor_);
  LOG_SENSOR("  ", "Battery level", this->battery_level_sensor_);
  LOG_SENSOR("  ", "Input voltage", this->input_voltage_sensor_);
#endif
}

}  // namespace m5pm1
}  // namespace esphome

#endif  // USE_ESP32
