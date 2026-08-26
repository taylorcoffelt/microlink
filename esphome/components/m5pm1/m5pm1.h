#pragma once

#ifdef USE_ESP32

#include "esphome/core/component.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace m5pm1 {

// Register map from m5stack/M5PM1, src/M5PM1.h. I2C address 0x6E.
static const uint8_t M5PM1_REG_GPIO_MODE = 0x10;   // [4:0] 1 = output
static const uint8_t M5PM1_REG_GPIO_OUT = 0x11;    // [4:0] output level
static const uint8_t M5PM1_REG_GPIO_DRV = 0x13;    // [4:0] 1 = open-drain
static const uint8_t M5PM1_REG_GPIO_FUNC0 = 0x16;  // 2 bits/pin, GPIO0..3

// PYG pin roles on the StickS3.
static const uint8_t M5PM1_PIN_LCD_RAIL = 2;    // PYG2 -> L3B: LCD, BL, mic, spk
static const uint8_t M5PM1_PIN_SPEAKER_AMP = 3;  // PYG3 -> amplifier gate

class M5PM1Component : public Component, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;

  // After the I2C bus (BUS = 1000), before the display initialises. Ordering is
  // the entire point: power the rail late and the panel init sequence talks to
  // a controller that has no power.
  float get_setup_priority() const override { return setup_priority::BUS - 1.0f; }

  void set_lcd_power(bool on) { this->lcd_power_ = on; }

  // Safe to call at runtime - gate the amp only while audio is actually
  // playing, otherwise it pops.
  void set_speaker_amp(bool on) {
    this->speaker_amp_ = on;
    if (this->is_ready())
      this->write_pin_(M5PM1_PIN_SPEAKER_AMP, on);
  }

 protected:
  bool update_bits_(uint8_t reg, uint8_t mask, uint8_t value);
  bool write_pin_(uint8_t pin, bool level);

  bool lcd_power_{true};
  bool speaker_amp_{false};
};

}  // namespace m5pm1
}  // namespace esphome

#endif  // USE_ESP32
