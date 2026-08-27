#pragma once

#ifdef USE_ESP32

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/components/i2c/i2c.h"

#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif

namespace esphome {
namespace m5pm1 {

// Register map from m5stack/M5PM1, src/M5PM1.h. I2C address 0x6E.
static const uint8_t M5PM1_REG_PWR_SRC = 0x04;     // [2:0] 0=5VIN 1=5VINOUT 2=BAT
static const uint8_t M5PM1_REG_PWR_OUT_EN = 0x06;  // 5V boost output enable
static const uint8_t M5PM1_REG_GPIO_MODE = 0x10;   // [4:0] 1 = output
static const uint8_t M5PM1_REG_GPIO_OUT = 0x11;    // [4:0] output level
static const uint8_t M5PM1_REG_GPIO_DRV = 0x13;    // [4:0] 1 = open-drain
static const uint8_t M5PM1_REG_GPIO_FUNC0 = 0x16;  // 2 bits/pin, GPIO0..3
static const uint8_t M5PM1_REG_VBAT_L = 0x22;      // battery mV, uint16 LE
static const uint8_t M5PM1_REG_VIN_L = 0x24;       // USB/DC input mV, uint16 LE
static const uint8_t M5PM1_REG_PULSE = 0x53;       // hardware 1-wire pulse macro

// Value M5Stack's own bring-up writes to 0x06 to bring the 5V boost up.
static const uint8_t M5PM1_PWR_OUT_ALL = 0x1F;

// 0x53 payload: refresh + 1 pulse + GPIO3. The PMIC emits the pulse train the
// AW8737 needs and then holds G3 high itself.
static const uint8_t M5PM1_PULSE_AMP_WAKE = 0xA3;

// PYG pin roles on the StickS3.
static const uint8_t M5PM1_PIN_LCD_RAIL = 2;     // PYG2 -> L3B: LCD, BL, codec
static const uint8_t M5PM1_PIN_SPEAKER_AMP = 3;  // PYG3 -> AW8737 enable

class M5PM1Component : public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;

  // After the I2C bus (BUS = 1000), before the display initialises. Ordering is
  // the entire point: power the rail late and the panel init sequence talks to
  // a controller that has no power.
  float get_setup_priority() const override { return setup_priority::BUS - 1.0f; }

  void set_lcd_power(bool on) { this->lcd_power_ = on; }

  // Safe to call at runtime - gate the amp only while audio is actually
  // playing, otherwise it pops and it wastes current.
  //
  // NB this is not a pin level. The AW8737 is a 1-wire gain-controlled part
  // that has to be pulsed awake; see apply_speaker_amp_() for why.
  void set_speaker_amp(bool on);

#ifdef USE_SENSOR
  void set_battery_voltage_sensor(sensor::Sensor *s) { this->battery_voltage_sensor_ = s; }
  void set_battery_level_sensor(sensor::Sensor *s) { this->battery_level_sensor_ = s; }
  void set_input_voltage_sensor(sensor::Sensor *s) { this->input_voltage_sensor_ = s; }
#endif

 protected:
  bool update_bits_(uint8_t reg, uint8_t mask, uint8_t value);
  bool write_pin_(uint8_t pin, bool level);
  bool apply_speaker_amp_(bool on);
  bool read_mv_(uint8_t reg_low, uint16_t *mv);
  static float battery_percent_(uint16_t mv);

  bool lcd_power_{true};
  bool speaker_amp_{false};

#ifdef USE_SENSOR
  sensor::Sensor *battery_voltage_sensor_{nullptr};
  sensor::Sensor *battery_level_sensor_{nullptr};
  sensor::Sensor *input_voltage_sensor_{nullptr};
#endif
};

}  // namespace m5pm1
}  // namespace esphome

#endif  // USE_ESP32
