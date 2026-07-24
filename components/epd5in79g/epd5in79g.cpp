#include "epd5in79g.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace epd5in79g {

static const char *const TAG = "epd5in79g";

// Byte-column widths, exactly as in Waveshare's Epd::Display():
//   Width  = EPD_WIDTH / 8   -> byte columns per RAM bank (2bpp, 4px/byte,
//                                 half the panel width per bank)
//   Half of the panel (396px) packed at 4px/byte = 99 bytes per bank.
static const uint16_t RAM_BANK_BYTE_WIDTH = 792 / 8;  // 99

void EPD5in79G::setup() {
  ESP_LOGCONFIG(TAG, "Setting up EPD5in79G...");

  this->dc_pin_->setup();
  this->dc_pin_->digital_write(false);
  if (this->reset_pin_ != nullptr) {
    this->reset_pin_->setup();
    this->reset_pin_->digital_write(true);
  }
  this->busy_pin_->setup();

  this->spi_setup();

  this->init_internal_(this->get_buffer_length_());
  this->fill(Color(255, 255, 255));

  this->reset_();
  this->init_sequence_();
}

void EPD5in79G::dump_config() {
  ESP_LOGCONFIG(TAG, "EPD5in79G:");
  ESP_LOGCONFIG(TAG, "  Resolution: %dx%d", EPD_WIDTH, EPD_HEIGHT);
  LOG_PIN("  CS Pin: ", this->cs_);
  LOG_PIN("  DC Pin: ", this->dc_pin_);
  LOG_PIN("  Busy Pin: ", this->busy_pin_);
  LOG_PIN("  Reset Pin: ", this->reset_pin_);
}

size_t EPD5in79G::get_buffer_length_() { return (size_t) EPD_WIDTH * (size_t) EPD_HEIGHT; }

void EPD5in79G::update() {
  this->do_update_();
  this->display_frame_();
}

// --- low level SPI helpers, ported from Epd::SendCommand / Epd::SendData ---

void EPD5in79G::send_command_(uint8_t command) {
  this->dc_pin_->digital_write(false);
  this->enable();
  this->write_byte(command);
  this->disable();
}

void EPD5in79G::send_data_(uint8_t data) {
  this->dc_pin_->digital_write(true);
  this->enable();
  this->write_byte(data);
  this->disable();
}

// Ported from Epd::ReadBusyH(): LOW = busy, HIGH = idle.
void EPD5in79G::wait_busy_high_() {
  uint32_t start = millis();
  while (this->busy_pin_->digital_read() == false) {
    delay(5);
    if (millis() - start > 30000) {
      ESP_LOGW(TAG, "Busy wait timed out");
      break;
    }
  }
}

// Ported from Epd::Reset().
void EPD5in79G::reset_() {
  if (this->reset_pin_ == nullptr)
    return;
  this->reset_pin_->digital_write(true);
  delay(20);
  this->reset_pin_->digital_write(false);
  delay(2);
  this->reset_pin_->digital_write(true);
  delay(20);
}

// Ported from Epd::Init(). Command bytes and ordering are unchanged from
// the vendor driver.
void EPD5in79G::init_sequence_() {
  this->send_command_(0xA2);
  this->send_data_(0x01);

  this->send_command_(0x00);
  this->send_data_(0x03);
  this->send_data_(0x29);

  this->send_command_(0xA2);
  this->send_data_(0x02);

  this->send_command_(0x00);
  this->send_data_(0x07);
  this->send_data_(0x29);

  this->send_command_(0xA2);
  this->send_data_(0x00);

  this->send_command_(0x50);
  this->send_data_(0x97);

  this->send_command_(0x61);
  this->send_data_(0x01);
  this->send_data_(0x8c);
  this->send_data_(0x01);
  this->send_data_(0x10);

  this->send_command_(0x06);
  this->send_data_(0x38);
  this->send_data_(0x38);
  this->send_data_(0x38);
  this->send_data_(0x00);

  this->send_command_(0xE9);
  this->send_data_(0x01);

  this->send_command_(0xE0);
  this->send_data_(0x01);

  this->send_command_(0x04);
  this->wait_busy_high_();
}

// Ported from Epd::TurnOnDisplay().
void EPD5in79G::turn_on_display_() {
  this->send_command_(0xA2);
  this->send_data_(0x00);

  this->send_command_(0x12);
  this->send_data_(0x00);
  this->wait_busy_high_();
}

// Ported from Epd::Sleep().
void EPD5in79G::deep_sleep_() {
  this->send_command_(0x07);
  this->send_data_(0xA5);
}

// --- pixel buffer handling ---

uint8_t EPD5in79G::get_pixel_color_(int x, int y) {
  if (x < 0 || x >= EPD_WIDTH || y < 0 || y >= EPD_HEIGHT)
    return COLOR_WHITE;
  return this->buffer_[y * EPD_WIDTH + x];
}

// Packs 4 horizontally-adjacent pixels into one byte, MSB = leftmost pixel.
// This matches the 4px/byte, 2 bits-per-pixel layout the vendor image
// converter produces for Epd::Display()'s Image[] argument.
uint8_t EPD5in79G::pack_byte_(int y, int byte_col) {
  int x0 = byte_col * 4;
  uint8_t c0 = this->get_pixel_color_(x0 + 0, y) & 0x03;
  uint8_t c1 = this->get_pixel_color_(x0 + 1, y) & 0x03;
  uint8_t c2 = this->get_pixel_color_(x0 + 2, y) & 0x03;
  uint8_t c3 = this->get_pixel_color_(x0 + 3, y) & 0x03;
  return (c0 << 6) | (c1 << 4) | (c2 << 2) | c3;
}

void EPD5in79G::draw_absolute_pixel_internal(int x, int y, Color color) {
  if (x < 0 || x >= EPD_WIDTH || y < 0 || y >= EPD_HEIGHT)
    return;

  uint8_t idx;
  if (color.red > 128 && color.green > 128 && color.blue < 80) {
    idx = COLOR_YELLOW;
  } else if (color.red > 128 && color.green < 80 && color.blue < 80) {
    idx = COLOR_RED;
  } else if (color.red < 80 && color.green < 80 && color.blue < 80) {
    idx = COLOR_BLACK;
  } else {
    idx = COLOR_WHITE;
  }
  this->buffer_[y * EPD_WIDTH + x] = idx;
}

void EPD5in79G::fill(Color color) {
  uint8_t idx;
  if (color.red > 128 && color.green > 128 && color.blue < 80) {
    idx = COLOR_YELLOW;
  } else if (color.red > 128 && color.green < 80 && color.blue < 80) {
    idx = COLOR_RED;
  } else if (color.red < 80 && color.green < 80 && color.blue < 80) {
    idx = COLOR_BLACK;
  } else {
    idx = COLOR_WHITE;
  }
  memset(this->buffer_, idx, this->get_buffer_length_());
}

// Ported from Epd::Display(). RAM1 holds the left half of the panel
// (columns 0-395), RAM2 the right half (columns 396-791), selected via
// the 0xA2 command exactly as in the vendor driver. Loop bounds and row
// ordering (including RAM2's full-height double pass) are kept identical
// to the reference implementation rather than "simplified", since that
// reference is the version confirmed working on this panel.
void EPD5in79G::display_frame_() {
  ESP_LOGD(TAG, "Sending frame to display...");

  // RAM1 - this bank is physically the RIGHT half of the screen (see
  // Epd::Display_part(): xstart>395 writes real data to RAM1 and blanks
  // RAM2), so it gets image columns 396-791.
  this->send_command_(0xA2);
  this->send_data_(0x01);
  this->send_command_(0x10);
  for (uint16_t j = 0; j < EPD_HEIGHT / 2; j++) {
    for (uint16_t bx = 0; bx < RAM_BANK_BYTE_WIDTH; bx++) {
      this->send_data_(this->pack_byte_(j, bx + RAM_BANK_BYTE_WIDTH));
    }
    for (uint16_t bx = 0; bx < RAM_BANK_BYTE_WIDTH; bx++) {
      this->send_data_(this->pack_byte_(EPD_HEIGHT - j - 1, bx + RAM_BANK_BYTE_WIDTH));
    }
  }

  // RAM2 - this bank is physically the LEFT half of the screen (see
  // Epd::Display_part(): xend<396 writes real data to RAM2 and blanks
  // RAM1), so it gets image columns 0-395.
  this->send_command_(0xA2);
  this->send_data_(0x02);
  this->send_command_(0x10);
  for (uint16_t j = 0; j < EPD_HEIGHT / 2; j++) {
    for (uint16_t bx = 0; bx < RAM_BANK_BYTE_WIDTH; bx++) {
      this->send_data_(this->pack_byte_(j, bx));
    }
    for (uint16_t bx = 0; bx < RAM_BANK_BYTE_WIDTH; bx++) {
      this->send_data_(this->pack_byte_(EPD_HEIGHT - j - 1, bx));
    }
  }

  this->turn_on_display_();
}

}  // namespace epd5in79g
}  // namespace esphome