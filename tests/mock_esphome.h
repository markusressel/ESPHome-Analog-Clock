#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <algorithm>

// Define ESPHome macros
#define USE_LIGHT 1
#define DISABLE_DITHERING 1
// Logging is a no-op on the host
#define ESP_LOGW(tag, ...) ((void)0)

namespace esphome {

extern uint32_t g_mock_millis;

inline uint32_t millis() {
    return g_mock_millis;
}

template <typename T>
inline T clamp(T val, T min, T max) {
    if (val < min) return min;
    if (val > max) return max;
    return val;
}

struct Color {
    union { uint8_t r; uint8_t red; };
    union { uint8_t g; uint8_t green; };
    union { uint8_t b; uint8_t blue; };
    union { uint8_t w; uint8_t white; };
    Color(uint8_t r_=0, uint8_t g_=0, uint8_t b_=0, uint8_t w_=0) : r(r_), g(g_), b(b_), w(w_) {}
    
    // Some minimal operator== for testing
    bool operator==(const Color& o) const {
        return r == o.r && g == o.g && b == o.b && w == o.w;
    }
};

namespace light {

struct LightColorValues {
    Color c;
    float br = 1.0f;
    float state = 1.0f;

    uint8_t get_red_raw() const { return c.r; }
    uint8_t get_green_raw() const { return c.g; }
    uint8_t get_blue_raw() const { return c.b; }
    uint8_t get_white_raw() const { return c.w; }

    float get_brightness() const { return br; }
    float get_state() const { return state; }
    float get_red() const { return c.r / 255.0f; }
    float get_green() const { return c.g / 255.0f; }
    float get_blue() const { return c.b / 255.0f; }
    float get_white() const { return c.w / 255.0f; }
    
    bool is_on() const { return state > 0.0f; }
};

class AddressableLightState {
public:
    LightColorValues remote_values;
    LightColorValues current_values;
    Color c;

    AddressableLightState() {}

    uint8_t get_red_raw() const { return c.r; }
    uint8_t get_green_raw() const { return c.g; }
    uint8_t get_blue_raw() const { return c.b; }
    uint8_t get_white_raw() const { return c.w; }

    float get_brightness() const { return remote_values.br; }
    float get_state() const { return remote_values.state; }
    float get_red() const { return remote_values.c.r / 255.0f; }
    float get_green() const { return remote_values.c.g / 255.0f; }
    float get_blue() const { return remote_values.c.b / 255.0f; }
    float get_white() const { return remote_values.c.w / 255.0f; }
    
    bool is_on() const { return remote_values.state > 0.0f; }

    void* get_output() { return nullptr; } // Dummy
    
    // AddressableLight view methods mock
    Color get() const { return c; }
};

struct LightStateView {
    AddressableLightState* state;
    uint8_t get_red_raw() const { return state->get_red_raw(); }
    uint8_t get_green_raw() const { return state->get_green_raw(); }
    uint8_t get_blue_raw() const { return state->get_blue_raw(); }
    uint8_t get_white_raw() const { return state->get_white_raw(); }
    void set_rgbw_raw(uint8_t r, uint8_t g, uint8_t b, uint8_t w) {
        state->c = Color(r, g, b, w);
    }
    
    void operator=(const Color& col) {
        state->c = col;
    }
    Color get() const { return state->c; }
};

class AddressableLight {
    std::vector<AddressableLightState> leds;
public:
    AddressableLight(int size) {
        leds.resize(size);
    }
    int size() const { return leds.size(); }
    
    LightStateView operator[](int i) {
        return LightStateView{&leds[i]};
    }

    void set_dithered_color(int i, float r, float g, float b, float w) {
        leds[i].c = Color(r * 255, g * 255, b * 255, w * 255);
    }
    
    void schedule_show() {}
    
    // Test helper to get direct state
    AddressableLightState& get_led(int i) { return leds[i]; }
};

class LightState {
public:
    LightColorValues remote_values;
    LightColorValues current_values;
};

} // namespace light

namespace number {
class Number {
public:
    float state;
    Number(float s = 0.0f) : state(s) {}
};
} // namespace number

namespace switch_ {
class Switch {
public:
    bool state;
    Switch(bool s = false) : state(s) {}
};
} // namespace switch_

namespace select {
class Select {
public:
    std::string state;
    Select(std::string s = "") : state(s) {}
    std::string current_option() const { return state; }
};
} // namespace select

} // namespace esphome

// To map `#include "esphome.h"` to our mock
#define ESPHOME_H

using namespace esphome;
using namespace esphome::light;
using namespace esphome::number;
using namespace esphome::select;
using namespace esphome::switch_;
