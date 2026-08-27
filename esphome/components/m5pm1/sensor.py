"""Voltage ADCs on the M5PM1.

The PMIC reports VBAT, VIN and 5VINOUT in millivolts. It has no coulomb
counter and no state-of-charge register, so `battery_level` is interpolated
from the battery voltage against a single-cell LiPo curve - useful, but an
estimate rather than a measurement.
"""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_BATTERY_LEVEL,
    CONF_BATTERY_VOLTAGE,
    DEVICE_CLASS_BATTERY,
    DEVICE_CLASS_VOLTAGE,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    UNIT_PERCENT,
    UNIT_VOLT,
)

from . import CONF_M5PM1_ID, M5PM1Component

DEPENDENCIES = ["m5pm1"]

CONF_INPUT_VOLTAGE = "input_voltage"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_M5PM1_ID): cv.use_id(M5PM1Component),
        cv.Optional(CONF_BATTERY_VOLTAGE): sensor.sensor_schema(
            unit_of_measurement=UNIT_VOLT,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_VOLTAGE,
            state_class=STATE_CLASS_MEASUREMENT,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
        cv.Optional(CONF_BATTERY_LEVEL): sensor.sensor_schema(
            unit_of_measurement=UNIT_PERCENT,
            accuracy_decimals=0,
            device_class=DEVICE_CLASS_BATTERY,
            state_class=STATE_CLASS_MEASUREMENT,
        ),
        # VIN is the USB/DC input. Non-zero means it is on external power, which
        # is also why the battery percentage will read high at the same time.
        cv.Optional(CONF_INPUT_VOLTAGE): sensor.sensor_schema(
            unit_of_measurement=UNIT_VOLT,
            accuracy_decimals=3,
            device_class=DEVICE_CLASS_VOLTAGE,
            state_class=STATE_CLASS_MEASUREMENT,
            entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
        ),
    }
)


async def to_code(config):
    parent = await cg.get_variable(config[CONF_M5PM1_ID])

    if CONF_BATTERY_VOLTAGE in config:
        sens = await sensor.new_sensor(config[CONF_BATTERY_VOLTAGE])
        cg.add(parent.set_battery_voltage_sensor(sens))

    if CONF_BATTERY_LEVEL in config:
        sens = await sensor.new_sensor(config[CONF_BATTERY_LEVEL])
        cg.add(parent.set_battery_level_sensor(sens))

    if CONF_INPUT_VOLTAGE in config:
        sens = await sensor.new_sensor(config[CONF_INPUT_VOLTAGE])
        cg.add(parent.set_input_voltage_sensor(sens))
