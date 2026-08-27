#include "m5pm1.h"

#ifdef USE_ESP32

#include "esphome/core/log.h"
#include "esphome/core/hal.h"

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

// The speaker amplifier is an AW8737A on PYG3, and its enable is a plain held
// level - not a pulse train, despite the PMIC offering a pulse macro at 0x53.
//
// From the AW8737A datasheet: a rising edge on SHDN enters Mode 1, and N
// rising edges within the timing window select Mode N (max 4). But modes 1-4
// all share the SAME voltage gain - 16.3 V/V at Rin=3k - and differ only in
// the boost converter's output power ceiling (1.2W down to 0.6W). There is no
// mode that is "on but inaudible", so selecting one buys nothing here.
//
// What does matter: SHDN must stay high. Pull it low for around 1ms and the
// amplifier shuts down. A pulse macro that returns the pin to its resting
// state is therefore an excellent way to turn the amplifier off again by
// accident.
//
// M5Unified's own StickS3 support does exactly what this does and nothing
// more: configure PM1 GPIO3 as a push-pull output once, then set or clear
// register 0x11 bit 3.
bool M5PM1Component::apply_speaker_amp_(bool on) {
  return this->write_pin_(M5PM1_PIN_SPEAKER_AMP, on);
}

void M5PM1Component::set_speaker_amp(bool on) {
  this->speaker_amp_ = on;
  if (this->is_ready())
    this->apply_speaker_amp_(on);
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

  // PWR_CFG: [4] LED_EN, [3] BOOST_EN, [2] LDO_EN, [1] DCDC_EN, [0] CHG_EN.
  // Only the low four - deliberately leaving LED_EN alone, since lighting an
  // LED on a battery device is not something this component should decide.
  //
  // Doubles as the presence check: if the PMIC does not answer, the LCD rail is
  // never coming up and the panel stays dark.
  if (!this->update_bits_(M5PM1_REG_PWR_OUT_EN, 0x0F, 0x0F)) {
    ESP_LOGE(TAG, "No response at 0x%02X - LCD rail will stay off", this->address_);
    this->mark_failed();
    return;
  }

  // Read-modify-write from here so the PYG pins we are not responsible for
  // keep whatever the PMIC or a previous boot left them at.

  // GPIO_FUNC0 packs two bits per pin: GPIO2 at [5:4], GPIO3 at [7:6].
  // 00 = plain GPIO rather than IRQ/WAKE/PWM.
  this->update_bits_(M5PM1_REG_GPIO_FUNC0, 0xF0, 0x00);

  // Push-pull, both pins.
  this->update_bits_(M5PM1_REG_GPIO_DRV, both, 0x00);

  // Both are plain manual outputs. G3's enable is a held level, so nothing
  // else may own its direction.
  this->update_bits_(M5PM1_REG_GPIO_MODE, both, both);

  this->write_pin_(M5PM1_PIN_LCD_RAIL, this->lcd_power_);
  this->write_pin_(M5PM1_PIN_SPEAKER_AMP, this->speaker_amp_);

  ESP_LOGI(TAG, "Rails up, L3B %s (PYG2), speaker amp %s (PYG3)",
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
  ESP_LOGCONFIG(TAG, "  Speaker amp (PYG3): %s (AW8737A, held level)",
                ONOFF(this->speaker_amp_));
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
