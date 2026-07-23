#pragma once

// Custom ESPHome display component for the Waveshare 5.79" G (4-colour,
// dual-RAM-bank) e-paper panel, ported directly from Waveshare's Arduino
// driver (epd5in79g.cpp / epd5in79g.h) so the SPI command sequence, the
// 0xA2 RAM1/RAM2 bank-select protocol, and the row order for each bank
// match the vendor's known-good implementation exactly. This avoids the
// vertical seam/tearing that comes from using a generic single-RAM-bank
// driver (e.g. the built-in 1.54in-G model) with this panel's dimensions.

#include "esphome/core/component.h"
#include "esphome/components/display/display_buffer.h"
#include "esphome/components/spi/spi.h"

namespace esphome {
namespace epd5in79g {

// 2-bit colour indices used by this panel. This ordering (BLACK/WHITE/
// YELLOW/RED = 0/1/2/3) is the common Waveshare 4-colour "G" panel mapping.
// If colours come out swapped on real hardware, adjust COLOR_* below.
enum EPD5IN79G_COLOR : uint8_t {
  COLOR_BLACK = 0x0,
  COLOR_WHITE = 0x1,
  COLOR_YELLOW = 0x2,
  COLOR_RED = 0x3,
};

class EPD5in79G : public display::DisplayBuffer,
                   public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW,
                                         spi::CLOCK_PHASE_LEADING, spi::DATA_RATE_2MHZ> {
 public:
  void set_dc_pin(GPIOPin *dc_pin) { this->dc_pin_ = dc_pin; }
  void set_busy_pin(GPIOPin *busy_pin) { this->busy_pin_ = busy_pin; }
  void set_reset_pin(GPIOPin *reset_pin) { this->reset_pin_ = reset_pin; }

  float get_setup_priority() const override { return setup_priority::PROCESSOR; }

  void setup() override;
  void dump_config() override;
  void update() override;
  void fill(Color color) override;

  display::DisplayType get_display_type() override { return display::DisplayType::DISPLAY_TYPE_COLOR; }

 protected:
  // --- display::DisplayBuffer overrides ---
  void draw_absolute_pixel_internal(int x, int y, Color color) override;
  int get_width_internal() override { return EPD_WIDTH; }
  int get_height_internal() override { return EPD_HEIGHT; }
  size_t get_buffer_length_();

  // --- ported from Waveshare Epd::* ---
  void reset_();
  void init_sequence_();
  void send_command_(uint8_t command);
  void send_data_(uint8_t data);
  void wait_busy_high_();
  void turn_on_display_();
  void display_frame_();
  void deep_sleep_();

  // Returns the 2-bit colour index (0-3) stored for pixel (x, y).
  uint8_t get_pixel_color_(int x, int y);
  // Packs 4 horizontally-adjacent pixels starting at pixel column
  // `byte_col * 4` in row `y` into a single byte (MSB = leftmost pixel),
  // matching the vendor image format consumed by Epd::Display().
  uint8_t pack_byte_(int y, int byte_col);

  static const uint16_t EPD_WIDTH = 792;
  static const uint16_t EPD_HEIGHT = 272;

  GPIOPin *dc_pin_{nullptr};
  GPIOPin *busy_pin_{nullptr};
  GPIOPin *reset_pin_{nullptr};
};

}  // namespace epd5in79g
}  // namespace esphome
