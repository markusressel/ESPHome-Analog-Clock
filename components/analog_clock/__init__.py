import esphome.codegen as cg
import esphome.config_validation as cv

# Makes the clock code (clock.h) available to the lambdas of analog_clock.yaml
CODEOWNERS = ["@markusressel"]

CONFIG_SCHEMA = cv.Schema({})


async def to_code(config):
    cg.add_global(cg.RawStatement('#include "esphome/components/analog_clock/clock.h"'))
