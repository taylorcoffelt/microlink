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

void M5PM1Component::dump_config() {
  ESP_LOGCONFIG(TAG, "M5PM1 PMIC:");
  LOG_I2C_DEVICE(this);
  ESP_LOGCONFIG(TAG, "  LCD rail (L3B/PYG2): %s", ONOFF(this->lcd_power_));
  ESP_LOGCONFIG(TAG, "  Speaker amp (PYG3): %s", ONOFF(this->speaker_amp_));
  if (this->is_failed())
    ESP_LOGE(TAG, "  Communication failed - the panel will be dark");
}

}  // namespace m5pm1
}  // namespace esphome

#endif  // USE_ESP32
