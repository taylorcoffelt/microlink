"""M5PM1 PMIC - the power controller on the M5Stack StickS3.

The reason this exists is rail L3B. On the StickS3 the LCD, its backlight, the
microphone and the speaker all sit downstream of L3B, switched by the PMIC's
own GPIO2 (PYG2). The PMIC brings up L0/L1/L2 by itself, so the ESP32-S3 boots
and runs happily - but L3B is left to software. M5Unified turns it on during
init; nothing in ESPHome does. Without an equivalent here the panel has no
power at all, and driving the GPIO38 backlight accomplishes nothing.

PYG3 gates the speaker amplifier and is left off by default.

Also exposes the PMIC's voltage ADCs - see sensor.py.
"""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import i2c
from esphome.const import CONF_ID

DEPENDENCIES = ["i2c"]
CODEOWNERS = ["@taylorcoffelt"]

CONF_M5PM1_ID = "m5pm1_id"
CONF_LCD_POWER = "lcd_power"
CONF_SPEAKER_AMP = "speaker_amp"

m5pm1_ns = cg.esphome_ns.namespace("m5pm1")
M5PM1Component = m5pm1_ns.class_(
    "M5PM1Component", cg.PollingComponent, i2c.I2CDevice
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(M5PM1Component),
            # PYG2 -> rail L3B: LCD + backlight + mic + speaker.
            cv.Optional(CONF_LCD_POWER, default=True): cv.boolean,
            # PYG3 -> speaker amplifier gate. Off by default: hold it high all
            # the time and you get a pop. Drive it only while playing.
            cv.Optional(CONF_SPEAKER_AMP, default=False): cv.boolean,
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x6E))
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)
    cg.add(var.set_lcd_power(config[CONF_LCD_POWER]))
    cg.add(var.set_speaker_amp(config[CONF_SPEAKER_AMP]))
