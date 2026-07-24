import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import spi, display
from esphome.const import CONF_ID, CONF_DC_PIN, CONF_BUSY_PIN, CONF_RESET_PIN

DEPENDENCIES = ["spi"]
AUTO_LOAD = ["display"]

epd5in79g_ns = cg.esphome_ns.namespace("epd5in79g")
EPD5in79G = epd5in79g_ns.class_(
    "EPD5in79G", cg.PollingComponent, spi.SPIDevice, display.DisplayBuffer
)

CONFIG_SCHEMA = cv.All(
    display.FULL_DISPLAY_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(EPD5in79G),
            cv.Required(CONF_DC_PIN): pins.gpio_output_pin_schema,
            cv.Required(CONF_BUSY_PIN): pins.gpio_input_pin_schema,
            cv.Optional(CONF_RESET_PIN): pins.gpio_output_pin_schema,
        }
    )
    .extend(cv.polling_component_schema("180s"))
    .extend(spi.spi_device_schema(cs_pin_required=True)),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await display.register_display(var, config)
    await spi.register_spi_device(var, config)

    dc_pin = await cg.gpio_pin_expression(config[CONF_DC_PIN])
    cg.add(var.set_dc_pin(dc_pin))

    busy_pin = await cg.gpio_pin_expression(config[CONF_BUSY_PIN])
    cg.add(var.set_busy_pin(busy_pin))

    if CONF_RESET_PIN in config:
        reset_pin = await cg.gpio_pin_expression(config[CONF_RESET_PIN])
        cg.add(var.set_reset_pin(reset_pin))