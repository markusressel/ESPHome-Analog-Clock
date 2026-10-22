#pragma once
#include "esphome/components/light/addressable_light.h"

namespace esphome {
namespace virtual_addressable_light {

class VirtualAddressableLight : public light::AddressableLight {
 public:
  void set_num_leds(int num_leds) {
    num_leds_ = num_leds;
    buf_ = new uint8_t[num_leds * 4]();
    effect_data_ = new uint8_t[num_leds]();
  }

  int32_t size() const override { return num_leds_; }

  light::LightTraits get_traits() override {
    auto traits = light::LightTraits();
    traits.set_supported_color_modes({light::ColorMode::RGB});
    return traits;
  }

  void write_state(light::LightState* state) override { this->mark_shown_(); }

  void clear_effect_data() override {
    for (int i = 0; i < this->size(); i++) {
      this->effect_data_[i] = 0;
    }
  }

  light::ESPColorView get_view_internal(int32_t index) const override {
    return light::ESPColorView(&buf_[index * 4 + 0], &buf_[index * 4 + 1], &buf_[index * 4 + 2], &buf_[index * 4 + 3],
                               &effect_data_[index], &this->correction_);
  }

 protected:
  int num_leds_ = 0;
  uint8_t* buf_ = nullptr;
  uint8_t* effect_data_ = nullptr;
};

}  // namespace virtual_addressable_light
}  // namespace esphome
