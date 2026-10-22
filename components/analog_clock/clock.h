#pragma once

// This header is included before any component, so it includes what it uses (the tests mock these with esphome.h)
#if __has_include("esphome/components/light/addressable_light.h")
#include "esphome/core/hal.h"
#include "esphome/core/color.h"
#include "esphome/components/light/addressable_light.h"
#include "esphome/components/light/light_state.h"
#include "esphome/components/switch/switch.h"
#else
#include "esphome.h"
#endif
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

// Composes the analog clock face, the user facing light and effects triggered by API actions onto the physical LED
// ring, with these rules:
// - the physical ring is internal, ALWAYS_ON, without transitions and with `gamma_correct: 0`
// - the user facing light is a virtual light with `gamma_correct: 1.0`, so its buffer holds perceived brightness
// - blending happens in linear light, gamma is applied exactly once in encode_output(), and a lit pixel never turns off

// Linear light color, 0-1 per channel
struct ClockColor {
  float r, g, b;
  ClockColor(float r = 0, float g = 0, float b = 0) : r(r), g(g), b(b) {}

  ClockColor operator+(const ClockColor &o) const { return ClockColor(r + o.r, g + o.g, b + o.b); }
  ClockColor operator*(float t) const { return ClockColor(r * t, g * t, b * t); }
  ClockColor lerp(const ClockColor &target, float t) const {
    return ClockColor(r + (target.r - r) * t, g + (target.g - g) * t, b + (target.b - b) * t);
  }
  float peak() const { return std::max({r, g, b}); }
};

// How the hands move, selected with the "Clock Hand Style" select (option names in hand_style_from_name())
enum class HandStyle {
  TRAIL,   // hands jump from LED to LED and leave a fading trail behind
  JUMP,    // hands jump from LED to LED without a trail
  SMOOTH,  // hands glide continuously, blended between neighbouring LEDs
  COMET,   // like SMOOTH, with a comet tail behind the second hand
};

inline HandStyle hand_style_from_name(const std::string &name) {
  if (name == "Jump") return HandStyle::JUMP;
  if (name == "Smooth") return HandStyle::SMOOTH;
  if (name == "Comet") return HandStyle::COMET;
  return HandStyle::TRAIL;
}

// The whole ring pulses in a color, see ClockCompositor::trigger_pulse()
struct ClockPulse {
  uint32_t start_time;  // 0 = inactive
  ClockColor hue;       // linear, normalized to a peak of 1
  float brightness;     // perceived, 0-1
  int count;
  float duration;  // ms per pulse
};

// A ring of light spreading in both directions from a position, see ClockCompositor::trigger_ripple()
struct ClockRipple {
  uint32_t start_time;  // 0 = inactive
  float origin;         // LED position
  ClockColor hue;
  float brightness;
  float speed;         // LEDs per second
  float max_distance;  // LEDs
};

// A comet circling the ring, see ClockCompositor::trigger_spin()
struct ClockSpin {
  uint32_t start_time;  // 0 = inactive
  bool rainbow;
  ClockColor hue;
  float brightness;
  float speed;  // rotations per second
  float rotations;
  float direction;  // 1 = clockwise, -1 = counterclockwise
  float tail;       // LEDs
};

// What the clock face area shows, see ClockCompositor::show_*()
enum class ViewKind {
  CLOCK,      // the clock face
  PROGRESS,   // colored sections of the ring, e.g. a progress arc
  COUNTDOWN,  // an arc that drains back to 12 o'clock while the time runs out, then blinks until another view is shown
  ALERT,      // the whole ring breathes in a color
};

// How the old view makes way for a new one of another kind
enum class ViewTransition {
  NONE,
  FADE,  // cross-fade
  WIPE,  // the new view sweeps clockwise around the ring from 12 o'clock
};

// "fade", "wipe", anything else is none (an empty name is the default transition, see clock_transition())
inline ViewTransition view_transition_from_name(const std::string &name) {
  if (name == "fade") return ViewTransition::FADE;
  if (name == "wipe") return ViewTransition::WIPE;
  return ViewTransition::NONE;
}

// Option names of the "Clock View Transition" select
inline ViewTransition view_transition_from_option(const std::string &option) {
  if (option == "Fade") return ViewTransition::FADE;
  if (option == "Wipe") return ViewTransition::WIPE;
  return ViewTransition::NONE;
}

// How a progress view changes when it is shown again with other sections
enum class ProgressAnimation {
  FROM_ENTITY,  // the one chosen with the "Clock View Progress Animation" entities
  NONE,     // the new sections appear at once
  FADE,     // the old look cross-fades into the new one
  FILL,     // the boundaries of the sections move smoothly (a different number of sections fades instead)
  DROP,     // new LEDs fall in from the end of the ring like drops of an hourglass, removed ones float up
};

// "none", "fade", "fill", "drop", anything else is the default (an empty name is the default animation)
inline ProgressAnimation progress_animation_from_name(const std::string &name) {
  if (name == "none") return ProgressAnimation::NONE;
  if (name == "fade") return ProgressAnimation::FADE;
  if (name == "fill") return ProgressAnimation::FILL;
  if (name == "drop") return ProgressAnimation::DROP;
  return ProgressAnimation::FROM_ENTITY;
}

// Option names of the "Clock View Progress Animation" select
inline ProgressAnimation progress_animation_from_option(const std::string &option) {
  if (option == "Fade") return ProgressAnimation::FADE;
  if (option == "Fill") return ProgressAnimation::FILL;
  if (option == "Drop") return ProgressAnimation::DROP;
  return ProgressAnimation::NONE;
}

// A part of the ring in a color, as fractions of the ring clockwise from 12 o'clock (0-1)
struct ClockSection {
  float start, end;
  ClockColor hue;  // linear, normalized to a peak of 1
};

struct ClockView {
  ViewKind kind = ViewKind::CLOCK;
  // the hands (and indicators) are drawn over the other kinds of views
  bool show_clock = false;
  // perceived 0-1, scaled by the brightness of the light
  float brightness = 1.0f;
  // perceived level of white that marks the rest of the ring for PROGRESS and COUNTDOWN (0 = none, < 0 = the default
  // track, which can change while the view is shown)
  float track = -1.0f;
  // PROGRESS
  std::vector<ClockSection> sections;
  // PROGRESS and COUNTDOWN: how the arc changes (a countdown can only drop its LEDs)
  ProgressAnimation animation = ProgressAnimation::FROM_ENTITY;
  // PROGRESS: blink like a finished countdown once the whole ring is filled
  bool blink_when_complete = false;
  // COUNTDOWN: the time started at `start_time` (ms), lasts `duration` ms
  uint32_t start_time = 0;
  float duration = 0.0f;
  // COUNTDOWN and ALERT
  ClockColor hue = ClockColor(1, 1, 1);
  // ALERT: ms per breath
  float period = 2000.0f;
};

class ClockCompositor {
 public:
  static constexpr uint32_t MIN_FRAME_TIME_MS = 20;
  static constexpr float GAMMA = 2.8f;
  static constexpr float INV_GAMMA = 1.0f / GAMMA;
  // Pixels whose perceived peak is below half of the smallest storable step are treated as black
  static constexpr float BLACK_THRESHOLD = 0.5f / 255.0f;
  // Time a full on/off or brightness change of the clock takes, in seconds
  static constexpr float FADE_TIME_S = 1.0f;
  // Without a frame of the "Clock" effect for this long, the light shows another effect (or a plain color)
  static constexpr uint32_t CLOCK_EFFECT_TIMEOUT_MS = 250;

  // Perceived brightness of the hour indicators
  static constexpr float INDICATOR_LEVEL = 1.0f / 3.0f;
  // Trail decay, as the original effect did it in perceived bytes: 2% per 32ms frame, at least one step
  static constexpr float TRAIL_DECAY_PER_MS = 0.02f / 32.0f;
  static constexpr float TRAIL_MIN_DECAY_PER_MS = (1.0f / 255.0f) / 32.0f;
  static constexpr float COMET_TAIL_LEDS = 8.0f;

  static constexpr int MAX_PULSES = 2;
  static constexpr int MAX_RIPPLES = 4;
  static constexpr int MAX_SPINS = 2;
  static constexpr float DEFAULT_PULSE_DURATION_MS = 1000.0f;
  static constexpr float DEFAULT_RIPPLE_SPEED = 20.0f;
  static constexpr float RIPPLE_RING_THICKNESS = 2.0f;
  static constexpr float DEFAULT_SPIN_SPEED = 1.0f;
  static constexpr float DEFAULT_SPIN_TAIL = 10.0f;
  static constexpr float DEFAULT_ALERT_PERIOD_MS = 2000.0f;
  static constexpr float DEFAULT_TRANSITION_MS = 600.0f;
  static constexpr float DEFAULT_PROGRESS_ANIMATION_MS = 800.0f;
  // The time between two drops of the DROP animation
  static constexpr float DROP_STAGGER_MS = 120.0f;
  // The time a drop is smeared over: a bit more than a frame
  static constexpr float DROP_TRAIL_MS = 32.0f;
  // The dimmest level of an alert's breath, perceived
  static constexpr float ALERT_FLOOR = 0.1f;
  // The "time is up" animation of a countdown: the whole ring blinks
  static constexpr float TIMER_UP_PERIOD_MS = 1000.0f;

  // offset: LED index at 12 o'clock
  // max_brightness: perceived output at full brightness (like color_correct, applied before gamma)
  // min_output: the smallest output byte that still lights an LED. With a `color_correct` on the physical light, which
  // scales every byte down, small bytes round to 0: min_output = ceil(256 / (color_correct byte + 1)).
  ClockCompositor(esphome::light::AddressableLight *physical, esphome::light::AddressableLight *virtual_main,
                  esphome::light::LightState *main_state, esphome::switch_::Switch *seconds_switch, esphome::switch_::Switch *indicators_switch,
                  esphome::switch_::Switch *wide_hour_hand_switch, int offset = 0, float max_brightness = 1.0f,
                  int min_output = 1)
      : physical_(physical),
        virtual_main_(virtual_main),
        main_state_(main_state),
        seconds_switch_(seconds_switch),
        indicators_switch_(indicators_switch),
        wide_hour_hand_switch_(wide_hour_hand_switch),
        offset_(offset) {
    num_leds_ = physical_->size();
    trail_.resize(num_leds_);
    clock_layer_.resize(num_leds_);
    view_layer_.resize(num_leds_);
    drop_kind_.assign(num_leds_, DROP_NONE);
    drop_launch_.assign(num_leds_, 0);
    drop_color_.resize(num_leds_);
    lit_prev_.assign(num_leds_, 0);
    lit_color_.resize(num_leds_);
    dots_.reserve(num_leds_);
    prev_layer_.resize(num_leds_);
    hands_.resize(num_leds_);
    max_output_ = std::pow(esphome::clamp(max_brightness, 0.0f, 1.0f), GAMMA) * 255.0f;
    // the perceived level that maps to the smallest lit output: the dimmest level the ring can show
    float min_lit = (float) std::max(min_output, 1);
    perceived_floor_ = max_output_ > min_lit ? std::pow(min_lit / max_output_, INV_GAMMA) : 1.0f;
    for (int i = 0; i < 256; i++) linear_lut_[i] = std::pow(i / 255.0f, GAMMA);
    for (auto &p : pulses_) p.start_time = 0;
    for (auto &r : ripples_) r.start_time = 0;
    for (auto &s : spins_) s.start_time = 0;
  }

  int num_leds() const { return num_leds_; }

  void set_hand_style(HandStyle style) { hand_style_ = style; }
  HandStyle hand_style() const { return hand_style_; }

  // Called by every frame of the "Clock" effect of the user facing light
  void mark_clock_effect() { last_clock_effect_frame_ = millis(); }

  // Perceived 0-1 to linear light
  static float to_linear(float perceived) { return std::pow(std::max(perceived, 0.0f), GAMMA); }

  // Maps a blended linear color to the output bytes. The pixel's peak is converted back to perceived brightness and
  // lifted onto [perceived_floor_, 1] before gamma is applied, so a lit pixel never turns off. All channels are scaled
  // by the same factor, which keeps the hue.
  esphome::Color encode_output(const ClockColor &color) const {
    float peak = std::min(color.peak(), 1.0f);
    if (peak <= 0.0f) return esphome::Color(0, 0, 0, 0);
    float perceived = std::pow(peak, INV_GAMMA);
    if (perceived < BLACK_THRESHOLD || max_output_ <= 0.0f) return esphome::Color(0, 0, 0, 0);
    float lifted = perceived_floor_ + (1.0f - perceived_floor_) * perceived;
    float scale = std::pow(lifted, GAMMA) * max_output_ / peak;
    auto to_byte = [&](float v) { return (uint8_t) (esphome::clamp(v * scale, 0.0f, 255.0f) + 0.5f); };
    return esphome::Color(to_byte(color.r), to_byte(color.g), to_byte(color.b), 0);
  }

  // A perceived RGB color like Home Assistant colors, as hue in linear light normalized to a peak of 1.
  // An empty color or [-1, ...] gives `fallback`.
  ClockColor hue_of(const std::vector<int> &color_rgb, const ClockColor &fallback) const {
    if (color_rgb.size() < 3 || color_rgb[0] < 0) return fallback;
    ClockColor c(linear_lut_[esphome::clamp(color_rgb[0], 0, 255)], linear_lut_[esphome::clamp(color_rgb[1], 0, 255)],
                 linear_lut_[esphome::clamp(color_rgb[2], 0, 255)]);
    float peak = c.peak();
    return peak > 0.0001f ? c * (1.0f / peak) : fallback;
  }

  // The color of views that don't get one: an r below 0 means the color of the light, see resolve_()
  static ClockColor light_color() { return ClockColor(-1.0f, -1.0f, -1.0f); }

  // The hue of an explicit color, or the color of the light for an empty color or [-1, ...]
  ClockColor hue_or_light_color(const std::vector<int> &color_rgb) const {
    if (color_rgb.size() < 3 || color_rgb[0] < 0) return light_color();
    return hue_of(color_rgb, ClockColor(1, 1, 1));
  }

  // `hue`, or the color of the light (as of this frame) if it is `light_color()`
  const ClockColor &resolve_(const ClockColor &hue) const { return hue.r < 0.0f ? light_hue_ : hue; }

  // LED position of a clock position (0-60, 0 = 12 o'clock)
  float led_position(float clock_position) const { return clock_position * num_leds_ / 60.0f; }

  // Distance between two LED positions around the ring
  float ring_distance(float a, float b) const {
    float d = std::fmod(std::fabs(a - b), (float) num_leds_);
    return std::min(d, num_leds_ - d);
  }

  // Signed distance from `from` to `to` around the ring, in (-num_leds / 2, num_leds / 2]
  float ring_offset(float from, float to) const {
    float d = std::fmod(to - from, (float) num_leds_);
    if (d > num_leds_ / 2.0f) d -= num_leds_;
    if (d <= -num_leds_ / 2.0f) d += num_leds_;
    return d;
  }

  // The whole ring pulses `count` times (<= 0: once) in the color, each pulse `duration` ms long (<= 0: default).
  // brightness: perceived 0-1 (<= 0: full), scaled by the light's brightness.
  void trigger_pulse(std::vector<int> color_rgb = {}, int count = 1, float duration = -1.0f,
                     float brightness = -1.0f) {
    ClockPulse &pulse = pulses_[pulse_head_];
    pulse_head_ = (pulse_head_ + 1) % MAX_PULSES;
    pulse.hue = hue_of(color_rgb, ClockColor(1, 1, 1));
    pulse.count = count > 0 ? count : 1;
    pulse.duration = duration > 0.0f ? duration : DEFAULT_PULSE_DURATION_MS;
    pulse.brightness = brightness > 0.0f ? std::min(brightness, 1.0f) : 1.0f;
    pulse.start_time = std::max<uint32_t>(millis(), 1);
  }

  // A ring of light spreads in both directions from a clock position (0-59, 0 = 12 o'clock, < 0: the second hand).
  // speed: LEDs per second, max_distance: LEDs (<= 0: half the ring), brightness as for trigger_pulse()
  void trigger_ripple(float position = 0.0f, std::vector<int> color_rgb = {}, float speed = -1.0f,
                      float max_distance = -1.0f, float brightness = -1.0f) {
    ClockRipple &ripple = ripples_[ripple_head_];
    ripple_head_ = (ripple_head_ + 1) % MAX_RIPPLES;
    ripple.origin = position < 0.0f ? second_position_ : led_position(position);
    ripple.hue = hue_of(color_rgb, ClockColor(1, 1, 1));
    ripple.speed = speed > 0.0f ? speed : DEFAULT_RIPPLE_SPEED;
    ripple.max_distance = max_distance > 0.0f ? max_distance : num_leds_ / 2.0f;
    ripple.brightness = brightness > 0.0f ? std::min(brightness, 1.0f) : 1.0f;
    ripple.start_time = std::max<uint32_t>(millis(), 1);
  }

  // A comet starts at 12 o'clock and circles the ring `rotations` times (<= 0: once), covering what it passes.
  // color_rgb: empty or [-1, ...] for a rainbow, speed: rotations per second, tail: LEDs, brightness as for
  // trigger_pulse()
  void trigger_spin(std::vector<int> color_rgb = {}, float rotations = -1.0f, float speed = -1.0f,
                    bool clockwise = true, float brightness = -1.0f, float tail = -1.0f) {
    ClockSpin &spin = spins_[spin_head_];
    spin_head_ = (spin_head_ + 1) % MAX_SPINS;
    spin.rainbow = color_rgb.size() < 3 || color_rgb[0] < 0;
    spin.hue = hue_of(color_rgb, ClockColor(1, 1, 1));
    spin.rotations = rotations > 0.0f ? rotations : 1.0f;
    spin.speed = speed > 0.0f ? speed : DEFAULT_SPIN_SPEED;
    spin.direction = clockwise ? 1.0f : -1.0f;
    spin.brightness = brightness > 0.0f ? std::min(brightness, 1.0f) : 1.0f;
    spin.tail = tail > 0.0f ? tail : DEFAULT_SPIN_TAIL;
    spin.start_time = std::max<uint32_t>(millis(), 1);
  }

  // Stops all running effects
  void clear_effects() {
    for (auto &p : pulses_) p.start_time = 0;
    for (auto &r : ripples_) r.start_time = 0;
    for (auto &s : spins_) s.start_time = 0;
  }

  bool has_active_effects() const {
    for (const auto &p : pulses_)
      if (p.start_time != 0) return true;
    for (const auto &r : ripples_)
      if (r.start_time != 0) return true;
    for (const auto &s : spins_)
      if (s.start_time != 0) return true;
    return false;
  }

  bool shows_clock() const { return shows_clock_; }

  // -- views: what the ring shows in place of the plain clock face. Only visible while the light shows the clock.
  // Showing a view of the kind that is shown already updates it in place, without a transition.

  ViewKind view_kind() const { return view_.kind; }

  // The transition views are shown with when the caller doesn't choose one (the "Clock View Transition" entities)
  void set_default_transition(ViewTransition transition, float transition_ms) {
    default_transition_ = transition;
    default_transition_ms_ = transition_ms > 0.0f ? transition_ms : DEFAULT_TRANSITION_MS;
  }
  ViewTransition default_transition() const { return default_transition_; }

  // The animation of progress views that don't choose one (the "Clock View Progress Animation" entities). `duration_ms`
  // is the time of a fade or fill, and of a drop for the whole ring.
  void set_default_progress_animation(ProgressAnimation animation, float duration_ms) {
    default_progress_animation_ = animation == ProgressAnimation::FROM_ENTITY ? ProgressAnimation::NONE : animation;
    default_progress_animation_ms_ = duration_ms > 0.0f ? duration_ms : DEFAULT_PROGRESS_ANIMATION_MS;
  }

  // The track of views that don't choose one (the "Clock View Track" entity), perceived 0-1
  void set_default_track(float track) { default_track_ = esphome::clamp(track, 0.0f, 1.0f); }

  // Seconds left of the shown countdown (0 once the time is up), NaN while no countdown is shown
  float countdown_remaining_s() const {
    if (view_.kind != ViewKind::COUNTDOWN) return NAN;
    float elapsed = (float) (millis() - view_.start_time);
    return std::max(view_.duration - elapsed, 0.0f) / 1000.0f;
  }

  // The name of the shown view, as in the "Clock View" select
  const char *view_name() const {
    switch (view_.kind) {
      case ViewKind::PROGRESS: return "Progress";
      case ViewKind::COUNTDOWN: return "Countdown";
      case ViewKind::ALERT: return "Alert";
      default: return "Clock";
    }
  }
  bool in_transition() const { return transition_.kind != ViewTransition::NONE; }

  void show_clock_face(ViewTransition transition = ViewTransition::NONE, float transition_ms = -1.0f) {
    ClockView view;
    view.show_clock = true;
    set_view_(view, transition, transition_ms);
  }

  // Sections of the ring in colors. Showing them again while a progress view is shown changes it with `animation`.
  // `sections` holds [start, end, r, g, b] per section: start and end are fractions of
  // the ring clockwise from 12 o'clock (0-1), the color is perceived 0-255. Later sections cover earlier ones.
  void show_progress(const std::vector<float> &sections, float track = -1.0f, bool show_clock = false,
                     float brightness = -1.0f, ViewTransition transition = ViewTransition::NONE,
                     float transition_ms = -1.0f, ProgressAnimation animation = ProgressAnimation::FROM_ENTITY,
                     bool blink_when_complete = false) {
    ClockView view;
    view.kind = ViewKind::PROGRESS;
    view.animation = animation;
    view.blink_when_complete = blink_when_complete;
    view.show_clock = show_clock;
    view.track = track < 0.0f ? -1.0f : esphome::clamp(track, 0.0f, 1.0f);
    view.brightness = brightness > 0.0f ? std::min(brightness, 1.0f) : 1.0f;
    for (size_t i = 0; i + 5 <= sections.size(); i += 5) {
      view.sections.push_back(ClockSection{esphome::clamp(sections[i], 0.0f, 1.0f),
                                           esphome::clamp(sections[i + 1], 0.0f, 1.0f),
                                           hue_or_light_color({(int) sections[i + 2], (int) sections[i + 3],
                                                               (int) sections[i + 4]})});
    }
    set_view_(view, transition, transition_ms);
  }

  // The arc drains back to 12 o'clock in `duration_s` (`remaining_s` <= 0: all of it is left). When the time is up,
  // the ring blinks until another view is shown.
  void show_countdown(float duration_s, float remaining_s = -1.0f, std::vector<int> color_rgb = {},
                      float track = -1.0f, bool show_clock = false, float brightness = -1.0f,
                      ViewTransition transition = ViewTransition::NONE, float transition_ms = -1.0f,
                      ProgressAnimation animation = ProgressAnimation::FROM_ENTITY) {
    ClockView view;
    view.kind = ViewKind::COUNTDOWN;
    view.animation = animation;
    view.show_clock = show_clock;
    view.track = track < 0.0f ? -1.0f : esphome::clamp(track, 0.0f, 1.0f);
    view.brightness = brightness > 0.0f ? std::min(brightness, 1.0f) : 1.0f;
    view.hue = hue_or_light_color(color_rgb);
    view.duration = std::max(duration_s, 0.001f) * 1000.0f;
    float left_ms = remaining_s > 0.0f ? std::min(remaining_s * 1000.0f, view.duration) : view.duration;
    view.start_time = std::max<uint32_t>(millis() - (uint32_t) (view.duration - left_ms), 1);
    set_view_(view, transition, transition_ms);
  }

  // The whole ring breathes in a color, `period_ms` per breath (<= 0: default)
  void show_alert(std::vector<int> color_rgb = {}, float period_ms = -1.0f, bool show_clock = false,
                  float brightness = -1.0f, ViewTransition transition = ViewTransition::NONE,
                  float transition_ms = -1.0f) {
    ClockView view;
    view.kind = ViewKind::ALERT;
    view.show_clock = show_clock;
    view.brightness = brightness > 0.0f ? std::min(brightness, 1.0f) : 1.0f;
    view.hue = hue_or_light_color(color_rgb);
    view.period = period_ms > 0.0f ? period_ms : DEFAULT_ALERT_PERIOD_MS;
    set_view_(view, transition, transition_ms);
  }

  void loop(bool time_valid, int hour, int minute, int second) {
    uint32_t now = millis();
    uint32_t dt = now - last_run_;
    if (last_run_ != 0 && dt < MIN_FRAME_TIME_MS) return;
    if (last_run_ == 0) dt = 0;
    last_run_ = now;

    update_fades_(dt);
    const auto &light = main_state_->remote_values;
    light_hue_ = hue_of({(int) (light.get_red() * 255.0f + 0.5f), (int) (light.get_green() * 255.0f + 0.5f),
                         (int) (light.get_blue() * 255.0f + 0.5f)},
                        ClockColor(1, 1, 1));
    update_clock_mode_(now);

    // the second hand moves within the second, so track when the second last changed
    if (second != last_second_) {
      last_second_ = second;
      second_start_ = now;
    }
    float second_fraction = std::min((now - second_start_) / 1000.0f, 0.999f);
    float smooth_second = second + second_fraction;
    second_position_ = led_position(smooth_second);

    for (auto &c : clock_layer_) c = ClockColor();
    if (shows_clock_ && time_valid) {
      render_clock_(hour, minute, second, smooth_second, dt);
    }
    update_view_(now);
    if (shows_clock_) {
      compose_views_(now);
    } else {
      displayed_arc_.clear();
      reset_drops_();
    }

    float base_level = to_linear(clock_brightness_ * on_fade_);
    float effect_level = clock_brightness_;
    prepare_effects_(now);

    for (int i = 0; i < num_leds_; i++) {
      ClockColor pixel;
      if (shows_clock_) {
        pixel = view_layer_[i] * base_level;
      } else {
        // the virtual light's buffer holds perceived values with its brightness already applied
        auto view = (*virtual_main_)[i];
        pixel = ClockColor(linear_lut_[view.get_red_raw()], linear_lut_[view.get_green_raw()],
                           linear_lut_[view.get_blue_raw()]) *
                to_linear(on_fade_);
      }
      pixel = apply_effects_(pixel, i, now, effect_level);
      (*physical_)[(i + offset_) % num_leds_] = encode_output(pixel);
    }
    physical_->schedule_show();
  }

 protected:
  // Fades the clock in and out with the light's state and follows brightness changes smoothly
  void update_fades_(uint32_t dt) {
    float target_on = main_state_->remote_values.get_state() > 0.0f ? 1.0f : 0.0f;
    float target_brightness = main_state_->remote_values.get_brightness();
    if (!fades_initialized_) {
      on_fade_ = target_on;
      clock_brightness_ = target_brightness;
      fades_initialized_ = true;
      return;
    }
    float step = dt / 1000.0f / FADE_TIME_S;
    on_fade_ = approach_(on_fade_, target_on, step);
    clock_brightness_ = approach_(clock_brightness_, target_brightness, step);
  }

  static float approach_(float value, float target, float step) {
    if (value < target) return std::min(value + step, target);
    return std::max(value - step, target);
  }

  // The light shows the clock while its "Clock" effect runs. ESPHome stops effects when the light turns off, so the
  // mode is only left while the light is on, which lets the clock fade out.
  void update_clock_mode_(uint32_t now) {
    bool clock_effect_running =
        last_clock_effect_frame_ != 0 && now - last_clock_effect_frame_ < CLOCK_EFFECT_TIMEOUT_MS;
    if (clock_effect_running) {
      shows_clock_ = true;
    } else if (main_state_->remote_values.get_state() > 0.0f) {
      shows_clock_ = false;
    }
  }

  // Weight of a hand at `position` for LED `i` when blending between neighbouring LEDs (width: LEDs at full weight)
  float hand_weight_(int i, float position, float width) const {
    float d = ring_distance((float) i, position);
    return esphome::clamp((width + 1.0f) / 2.0f - d, 0.0f, 1.0f);
  }

  // The clock face at full brightness, in linear light
  void render_clock_(int hour, int minute, int second, float smooth_second, uint32_t dt) {
    HandStyle style = hand_style_;
    std::vector<ClockColor> &hands = hands_;
    std::vector<ClockColor> &layer = clock_layer_;
    for (auto &h : hands) h = ClockColor();
    const ClockColor hour_color(0, 0, 1), minute_color(0, 1, 0), second_color(1, 0, 0);
    bool wide_hour = wide_hour_hand_switch_->state;
    bool show_seconds = seconds_switch_->state;
    float hour_width = wide_hour ? 3.0f : 1.0f;

    if (style == HandStyle::SMOOTH || style == HandStyle::COMET) {
      float smooth_minute = minute + smooth_second / 60.0f;
      float hour_position = ((hour % 12) + smooth_minute / 60.0f) * num_leds_ / 12.0f;
      float minute_position = led_position(smooth_minute);
      for (int i = 0; i < num_leds_; i++) {
        hands[i] = hands[i] + hour_color * hand_weight_(i, hour_position, hour_width);
        hands[i] = hands[i] + minute_color * hand_weight_(i, minute_position, 1.0f);
        if (!show_seconds) continue;
        if (style == HandStyle::SMOOTH) {
          hands[i] = hands[i] + second_color * hand_weight_(i, second_position_, 1.0f);
        } else {
          // the head blends into the next LED, the tail fades out (in perceived brightness) behind it
          float behind = ring_offset((float) i, second_position_);
          float level = 0.0f;
          if (behind < 0.0f) {
            level = std::max(0.0f, 1.0f + behind);
          } else if (behind < COMET_TAIL_LEDS) {
            level = to_linear(1.0f - behind / COMET_TAIL_LEDS);
          }
          hands[i] = hands[i] + second_color * level;
        }
      }
    } else {
      // the original positions: whole LEDs, the hour hand also moves with the minutes
      int hour_index = (int) ((hour % 12) * (num_leds_ / 12.0f)) + (int) ((minute / 12.0f) * (num_leds_ / 60.0f));
      int minute_index = (int) (minute * (num_leds_ / 60.0f));
      int second_index = (int) (second * (num_leds_ / 60.0f));
      for (int d = wide_hour ? -1 : 0; d <= (wide_hour ? 1 : 0); d++) {
        int i = ((hour_index + d) % num_leds_ + num_leds_) % num_leds_;
        hands[i] = hands[i] + hour_color;
      }
      hands[minute_index % num_leds_] = hands[minute_index % num_leds_] + minute_color;
      if (show_seconds) hands[second_index % num_leds_] = hands[second_index % num_leds_] + second_color;
    }

    if (style == HandStyle::TRAIL) {
      // decays in perceived brightness like the original effect, so the trail looks the same
      float rel = TRAIL_DECAY_PER_MS * dt, min_step = TRAIL_MIN_DECAY_PER_MS * dt;
      auto decay = [&](float v) { return std::max(0.0f, v - std::max(v * rel, min_step)); };
      for (int i = 0; i < num_leds_; i++) {
        ClockColor &t = trail_[i];
        t = ClockColor(decay(t.r), decay(t.g), decay(t.b));
        const ClockColor &h = hands[i];
        // hands at full strength, stored perceived
        t = ClockColor(std::max(t.r, std::pow(std::min(h.r, 1.0f), INV_GAMMA)),
                       std::max(t.g, std::pow(std::min(h.g, 1.0f), INV_GAMMA)),
                       std::max(t.b, std::pow(std::min(h.b, 1.0f), INV_GAMMA)));
        hands[i] = ClockColor(to_linear(t.r), to_linear(t.g), to_linear(t.b));
      }
    } else {
      for (auto &t : trail_) t = ClockColor();
    }

    bool indicators = indicators_switch_->state;
    float indicator = to_linear(INDICATOR_LEVEL);
    int indicator_step = std::max(1, num_leds_ / 12);
    for (int i = 0; i < num_leds_; i++) {
      layer[i] = hands[i];
      if (!indicators) continue;
      // twelve indicators like the original effect, covered by the hands
      if (i % indicator_step == 0 && i / indicator_step < 12) {
        float cover = std::min(hands[i].peak(), 1.0f);
        layer[i] = layer[i] + ClockColor(indicator, indicator, indicator) * (1.0f - cover);
      }
    }
  }

  void set_view_(const ClockView &view, ViewTransition transition, float transition_ms) {
    uint32_t now = millis();
    if (view.kind != view_.kind && transition != ViewTransition::NONE) {
      prev_view_ = view_;
      transition_.kind = transition;
      transition_.start_time = now;
      transition_.duration = transition_ms > 0.0f ? transition_ms : default_transition_ms_;
    } else if (view.kind != view_.kind) {
      transition_.kind = ViewTransition::NONE;
    }
    if (view.kind != view_.kind) {
      reset_drops_();
      blink_since_ = 0;
    }
    if (view.kind == ViewKind::PROGRESS) start_progress_animation_(view, now);
    view_ = view;
  }

  static bool same_sections_(const std::vector<ClockSection> &a, const std::vector<ClockSection> &b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); i++) {
      if (a[i].start != b[i].start || a[i].end != b[i].end || a[i].hue.r != b[i].hue.r || a[i].hue.g != b[i].hue.g ||
          a[i].hue.b != b[i].hue.b) {
        return false;
      }
    }
    return true;
  }

  // New sections of a progress view that is shown already move from what is displayed now
  void start_progress_animation_(const ClockView &view, uint32_t now) {
    ProgressAnimation animation = view.animation == ProgressAnimation::FROM_ENTITY ? default_progress_animation_ : view.animation;
    // other kinds of views make way with a transition; nothing to animate from if nothing was displayed (light off)
    // (drops don't start from a snapshot, they are tracked per LED)
    if (view_.kind != ViewKind::PROGRESS || animation == ProgressAnimation::NONE ||
        animation == ProgressAnimation::DROP || (int) displayed_arc_.size() != num_leds_) {
      progress_anim_.kind = ProgressAnimation::NONE;
      return;
    }
    // equal sections change nothing
    if (same_sections_(view.sections, view_.sections)) return;
    progress_anim_.kind = animation;
    progress_anim_.start_time = now;
    progress_anim_.duration = default_progress_animation_ms_;
    progress_anim_.from_sections = displayed_sections_;
    progress_anim_.from_layer = displayed_arc_;
  }

  // Ends transitions that are over
  void update_view_(uint32_t now) {
    if (transition_.kind != ViewTransition::NONE && now - transition_.start_time >= transition_.duration) {
      transition_.kind = ViewTransition::NONE;
    }
  }

  // Fills view_layer_ with the current view, blended with the previous one while a transition runs
  void compose_views_(uint32_t now) {
    render_view_(view_, now, view_layer_, true);
    if (transition_.kind == ViewTransition::NONE) return;
    render_view_(prev_view_, now, prev_layer_);
    float t = esphome::clamp((now - transition_.start_time) / transition_.duration, 0.0f, 1.0f);
    t = t * t * (3.0f - 2.0f * t);
    // the edge of the wipe is one LED wide
    float edge = t * (num_leds_ + 1);
    for (int i = 0; i < num_leds_; i++) {
      float cover = transition_.kind == ViewTransition::WIPE ? esphome::clamp(edge - i, 0.0f, 1.0f) : t;
      view_layer_[i] = prev_layer_[i].lerp(view_layer_[i], cover);
    }
  }

  // A view at full brightness, in linear light. `clock_layer_` is the clock face of this frame.
  // `current`: the view is the shown one (and not the previous one of a transition), which remembers its animation state
  void render_view_(const ClockView &view, uint32_t now, std::vector<ClockColor> &out, bool current = false) {
    if (view.kind == ViewKind::CLOCK) {
      out = clock_layer_;
      return;
    }
    float level = to_linear(view.brightness);
    ClockColor track = ClockColor(1, 1, 1) * to_linear((view.track < 0.0f ? default_track_ : view.track) * view.brightness);
    for (auto &c : out) c = view.kind == ViewKind::ALERT ? ClockColor() : track;

    if (view.kind == ViewKind::PROGRESS || view.kind == ViewKind::COUNTDOWN) {
      render_arc_(view, level, track, now, out, current);
    } else {
      float phase = std::fmod((now - alert_start_) / view.period, 1.0f);
      float breath = 0.5f - 0.5f * std::cos(phase * 2.0f * 3.14159265f);
      float perceived = ALERT_FLOOR + (1.0f - ALERT_FLOOR) * breath;
      for (auto &c : out) c = resolve_(view.hue) * to_linear(perceived * view.brightness);
    }

    if (view.show_clock) {
      for (int i = 0; i < num_leds_; i++) {
        float cover = std::min(clock_layer_[i].peak(), 1.0f);
        out[i] = out[i] * (1.0f - cover) + clock_layer_[i];
      }
    }
  }

  // A progress or countdown view over the track already in `out`, with what they have in common: the arc made of
  // sections, its animation when it changes, and the blink of a finished one.
  void render_arc_(const ClockView &view, float level, const ClockColor &track, uint32_t now,
                   std::vector<ClockColor> &out, bool current) {
    bool countdown = view.kind == ViewKind::COUNTDOWN;
    ProgressAnimation configured = view.animation == ProgressAnimation::FROM_ENTITY ? default_progress_animation_ : view.animation;
    // a countdown changes all the time, so it can only drop its LEDs (the other animations restart with every change)
    bool drops = configured == ProgressAnimation::DROP;
    ProgressAnimation animation = (current && !countdown && !drops) ? progress_anim_.kind : ProgressAnimation::NONE;
    float elapsed = (float) (now - progress_anim_.start_time);
    float t = animation == ProgressAnimation::NONE ? 1.0f : esphome::clamp(elapsed / progress_anim_.duration, 0.0f, 1.0f);
    float eased = t * t * (3.0f - 2.0f * t);

    const std::vector<ClockSection> *sections = &view.sections;
    bool time_is_up = false;
    if (countdown) {
      float spent = (float) (now - view.start_time);
      time_is_up = spent >= view.duration;
      countdown_sections_.clear();
      if (!time_is_up) countdown_sections_.push_back(ClockSection{0.0f, 1.0f - spent / view.duration, resolve_(view.hue)});
      sections = &countdown_sections_;
    } else if (animation == ProgressAnimation::FILL) {
      if (progress_anim_.from_sections.size() == view.sections.size()) {
        filled_sections_ = view.sections;
        for (size_t i = 0; i < filled_sections_.size(); i++) {
          const ClockSection &from = progress_anim_.from_sections[i];
          filled_sections_[i].start = from.start + (view.sections[i].start - from.start) * eased;
          filled_sections_[i].end = from.end + (view.sections[i].end - from.end) * eased;
          filled_sections_[i].hue = resolve_(from.hue).lerp(resolve_(view.sections[i].hue), eased);
        }
        sections = &filled_sections_;
      } else {
        animation = ProgressAnimation::FADE;
      }
    }
    // drops are whole LEDs, so the arc is made of whole LEDs, too (an edge LED is on from half of its length)
    for (const auto &section : *sections) draw_section_(out, section.start, section.end, resolve_(section.hue) * level, drops);

    if (animation == ProgressAnimation::FADE) {
      for (int i = 0; i < num_leds_; i++) out[i] = progress_anim_.from_layer[i].lerp(out[i], eased);
    }
    bool drops_in_flight = false;
    if (drops && current) drops_in_flight = update_drops_(out, track, 0.5f * level, now);
    if (!current) return;

    if (animation != ProgressAnimation::NONE && t >= 1.0f) progress_anim_.kind = ProgressAnimation::NONE;
    displayed_sections_ = *sections;
    displayed_arc_ = out;

    // a finished countdown blinks; so does a progress that filled the ring, if it should, once its LEDs arrived
    ClockColor blink_hue = resolve_(view.hue);
    uint32_t blink_start = 0;
    if (time_is_up) {
      blink_start = view.start_time + (uint32_t) view.duration;
    } else if (!countdown && view.blink_when_complete && !view.sections.empty() && !drops_in_flight) {
      float first = 1.0f, last = 0.0f;
      for (const auto &section : view.sections) {
        first = std::min(first, section.start);
        last = std::max(last, section.end);
      }
      if (first <= 0.001f && last >= 0.999f) {
        if (blink_since_ == 0) blink_since_ = std::max<uint32_t>(now, 1);
        blink_start = blink_since_;
        blink_hue = resolve_(view.sections.back().hue);
      }
    }
    if (blink_start == 0) {
      blink_since_ = 0;
      return;
    }
    // the ring blinks (smoothly, on for the first half of a period)
    float phase = std::fmod((float) (now - blink_start) / TIMER_UP_PERIOD_MS, 1.0f);
    float blink = esphome::clamp(std::sin(phase * 2.0f * 3.14159265f) * 4.0f + 0.5f, 0.0f, 1.0f);
    for (auto &c : out) c = blink_hue * (level * blink);
  }

  void reset_drops_() {
    drops_valid_ = false;
    std::fill(drop_kind_.begin(), drop_kind_.end(), DROP_NONE);
  }

  // The DROP animation, on top of the arc in `out`. Every LED that switches on or off since the last frame gets a drop
  // of its own: a new LED stays dark until a drop that fell from the end of the ring (just before 12 o'clock) arrives,
  // a LED that goes dark lifts off and floats up to the end of the ring. Drops are independent of each other, so they
  // can overlap, and a change doesn't affect the ones in flight. `lit_level`: from this peak an LED counts as lit.
  // Returns whether a new LED is still on its way.
  bool update_drops_(std::vector<ClockColor> &out, const ClockColor &dark, float lit_level, uint32_t now) {
    float travel = default_progress_animation_ms_;
    float top = (float) (num_leds_ - 1);
    float trail = DROP_TRAIL_MS * num_leds_ / travel;

    // the LEDs that changed
    int fresh = 0, gone = 0;
    for (int i = 0; i < num_leds_; i++) {
      bool lit = out[i].peak() >= lit_level && lit_level > 0.0f;
      if (drops_valid_ && lit != (bool) lit_prev_[i]) {
        // a LED that switches back while its drop is on its way just stays as it is
        if (lit && drop_kind_[i] != DROP_UP) fresh++;
        if (!lit && drop_kind_[i] != DROP_DOWN) gone++;
      }
    }
    // each new drop starts a bit after the one before: the lowest new LED first, the highest removed LED first
    float fresh_stagger = fresh > 1 ? std::min(DROP_STAGGER_MS, travel / fresh) : 0.0f;
    float gone_stagger = gone > 1 ? std::min(DROP_STAGGER_MS, travel / gone) : 0.0f;
    uint32_t next_down = std::max(now, next_down_);
    uint32_t next_up = std::max(now, next_up_);
    for (int i = 0; i < num_leds_; i++) {
      bool lit = out[i].peak() >= lit_level && lit_level > 0.0f;
      if (drops_valid_ && lit != (bool) lit_prev_[i]) {
        if (lit && drop_kind_[i] == DROP_UP) {
          // back on while it was lifting off: it just stays
          drop_kind_[i] = DROP_NONE;
        } else if (!lit && drop_kind_[i] == DROP_DOWN) {
          // off again before its drop arrived
          drop_kind_[i] = DROP_NONE;
        } else if (lit) {
          drop_kind_[i] = DROP_DOWN;
          drop_launch_[i] = next_down;
          next_down += (uint32_t) fresh_stagger;
          drop_color_[i] = out[i];
        } else {
          drop_kind_[i] = DROP_UP;
          // launched below, highest first
          drop_launch_[i] = 0;
          drop_color_[i] = lit_color_[i];
        }
      }
      lit_prev_[i] = lit;
      if (lit) lit_color_[i] = out[i];
    }
    for (int i = num_leds_ - 1; i >= 0; i--) {
      if (drop_kind_[i] == DROP_UP && drop_launch_[i] == 0) {
        drop_launch_[i] = next_up;
        next_up += (uint32_t) gone_stagger;
      }
    }
    next_down_ = next_down;
    next_up_ = next_up;
    drops_valid_ = true;

    // the drops
    bool in_flight = false;
    dots_.clear();
    for (int i = 0; i < num_leds_; i++) {
      if (drop_kind_[i] == DROP_NONE) continue;
      int32_t since_launch = (int32_t) (now - drop_launch_[i]);
      if (drop_kind_[i] == DROP_DOWN) {
        float position = top - since_launch / travel * num_leds_;
        if (since_launch >= 0 && position <= i) {
          drop_kind_[i] = DROP_NONE;
          continue;
        }
        in_flight = true;
        out[i] = dark;
        // falling: the smear is above the drop
        if (since_launch >= 0) dots_.push_back(Dot{position, std::min(position + trail, top), drop_color_[i]});
      } else {
        float position = i + since_launch / travel * num_leds_;
        if (since_launch < 0) {
          out[i] = drop_color_[i];
        } else if (position - trail >= num_leds_) {
          // all of it has left the ring
          drop_kind_[i] = DROP_NONE;
          drop_launch_[i] = 0;
          continue;
        } else {
          // lifting: the smear is below the drop, but not on the LED it left
          dots_.push_back(Dot{std::max(position - trail, (float) i), position, drop_color_[i]});
        }
      }
    }
    for (const auto &dot : dots_) draw_drop_(out, dot.from, dot.to, dot.color);
    return in_flight;
  }

  // A drop that covers the way from `from` to `to` (LED positions): an LED is lit by its distance to that way
  void draw_drop_(std::vector<ClockColor> &out, float from, float to, const ClockColor &color) const {
    int first = std::max((int) from - 1, 0), last = std::min((int) to + 2, num_leds_ - 1);
    for (int j = first; j <= last; j++) {
      float distance = j < from ? from - j : (j > to ? j - to : 0.0f);
      float weight = std::max(0.0f, 1.0f - distance);
      if (weight > 0.0f) out[j] = out[j].lerp(color, weight);
    }
  }

  // Covers the part of the ring between two fractions with a color; an LED the section only touches is blended
  void draw_section_(std::vector<ClockColor> &out, float start, float end, const ClockColor &color,
                     bool whole_leds = false) const {
    if (end <= start) return;
    for (int i = 0; i < num_leds_; i++) {
      float led_start = (float) i / num_leds_, led_end = (float) (i + 1) / num_leds_;
      float overlap = std::min(end, led_end) - std::max(start, led_start);
      if (overlap <= 0.0f) continue;
      float cover = std::min(overlap * num_leds_, 1.0f);
      if (whole_leds) cover = cover >= 0.5f ? 1.0f : 0.0f;
      out[i] = out[i].lerp(color, cover);
    }
  }

  // Removes finished effects
  void prepare_effects_(uint32_t now) {
    for (auto &p : pulses_) {
      if (p.start_time != 0 && now - p.start_time >= p.duration * p.count) p.start_time = 0;
    }
    for (auto &r : ripples_) {
      if (r.start_time == 0) continue;
      float radius = (now - r.start_time) / 1000.0f * r.speed;
      if (radius - RIPPLE_RING_THICKNESS > r.max_distance) r.start_time = 0;
    }
    for (auto &s : spins_) {
      if (s.start_time == 0) continue;
      float travelled = (now - s.start_time) / 1000.0f * s.speed * num_leds_;
      if (travelled > s.rotations * num_leds_ + s.tail) s.start_time = 0;
    }
  }

  // Adds pulses and ripples to a pixel and covers it with spins. `level` is the perceived brightness of the light.
  ClockColor apply_effects_(ClockColor pixel, int i, uint32_t now, float level) const {
    for (const auto &p : pulses_) {
      if (p.start_time == 0) continue;
      float phase = std::fmod((now - p.start_time) / p.duration, 1.0f);
      float envelope = std::sin(phase * 3.14159265f);
      pixel = pixel + p.hue * to_linear(p.brightness * level * envelope);
    }
    for (const auto &r : ripples_) {
      if (r.start_time == 0) continue;
      float radius = (now - r.start_time) / 1000.0f * r.speed;
      float distance = ring_distance((float) i, r.origin);
      if (distance > r.max_distance) continue;
      float diff = std::fabs(distance - radius);
      if (diff >= RIPPLE_RING_THICKNESS) continue;
      float profile = 1.0f - diff / RIPPLE_RING_THICKNESS;
      float fade = 1.0f - distance / (r.max_distance + 1.0f);
      pixel = pixel + r.hue * to_linear(r.brightness * level * profile * fade);
    }
    for (const auto &s : spins_) {
      if (s.start_time == 0) continue;
      float travelled = (now - s.start_time) / 1000.0f * s.speed * num_leds_;
      float end = s.rotations * num_leds_;
      // how far the LED is behind the head, counted along the way the comet goes
      float path = s.direction > 0.0f ? (float) i : (float) ((num_leds_ - i) % num_leds_);
      float behind = std::fmod(travelled - path, (float) num_leds_);
      if (behind < 0.0f) behind += num_leds_;
      float cover = 0.0f;
      if (behind > num_leds_ - 1.0f) {
        // just ahead of the head: blends in as the head approaches, unless the comet stops before it
        if (travelled + (num_leds_ - behind) <= end) cover = behind - (num_leds_ - 1.0f);
      } else if (behind < s.tail && travelled - behind >= 0.0f && travelled - behind <= end) {
        // the tail, as far as the comet has come (and not past the end of its travel)
        cover = 1.0f - behind / s.tail;
      }
      if (cover <= 0.0f) continue;
      ClockColor color = s.hue;
      if (s.rainbow) color = rainbow_(i);
      // smoothstep, so the tail fades out softly
      cover = cover * cover * (3.0f - 2.0f * cover);
      pixel = pixel.lerp(color * to_linear(s.brightness * level), cover);
    }
    return pixel;
  }

  // Pure hue in linear light for the LED's position around the ring
  ClockColor rainbow_(int i) const {
    float h = (float) i / num_leds_ * 6.0f;
    int sector = (int) h;
    float f = h - sector;
    float r, g, b;
    switch (sector % 6) {
      case 0: r = 1; g = f; b = 0; break;
      case 1: r = 1 - f; g = 1; b = 0; break;
      case 2: r = 0; g = 1; b = f; break;
      case 3: r = 0; g = 1 - f; b = 1; break;
      case 4: r = f; g = 0; b = 1; break;
      default: r = 1; g = 0; b = 1 - f; break;
    }
    return ClockColor(to_linear(r), to_linear(g), to_linear(b));
  }

  esphome::light::AddressableLight *physical_;
  esphome::light::AddressableLight *virtual_main_;
  esphome::light::LightState *main_state_;
  esphome::switch_::Switch *seconds_switch_;
  esphome::switch_::Switch *indicators_switch_;
  esphome::switch_::Switch *wide_hour_hand_switch_;
  int offset_;
  int num_leds_;

  float linear_lut_[256];
  float max_output_;
  float perceived_floor_;

  uint32_t last_run_ = 0;
  bool fades_initialized_ = false;
  float on_fade_ = 0.0f;
  float clock_brightness_ = 1.0f;
  uint32_t last_clock_effect_frame_ = 0;
  bool shows_clock_ = false;

  int last_second_ = -1;
  uint32_t second_start_ = 0;
  float second_position_ = 0.0f;
  HandStyle hand_style_ = HandStyle::TRAIL;
  // per LED, perceived per channel
  std::vector<ClockColor> trail_;
  // per frame buffers, allocated once
  std::vector<ClockColor> clock_layer_;
  std::vector<ClockColor> hands_;
  std::vector<ClockColor> view_layer_;
  std::vector<ClockColor> prev_layer_;

  ProgressAnimation default_progress_animation_ = ProgressAnimation::NONE;
  float default_progress_animation_ms_ = DEFAULT_PROGRESS_ANIMATION_MS;
  // the animation of the shown progress view, and what it displayed when the sections changed
  struct {
    ProgressAnimation kind = ProgressAnimation::NONE;
    uint32_t start_time = 0;
    float duration = 0.0f;
    std::vector<ClockSection> from_sections;
    std::vector<ClockColor> from_layer;
  } progress_anim_;
  std::vector<ClockSection> displayed_sections_;
  std::vector<ClockColor> displayed_arc_;
  std::vector<ClockSection> filled_sections_;
  std::vector<ClockSection> countdown_sections_;
  // a drop is smeared over the way it came since the last frame, so it doesn't skip LEDs
  struct Dot {
    float from, to;
    ClockColor color;
  };
  std::vector<Dot> dots_;
  // the drops: per LED whether it has one (to be reset when the arc is not shown), when it started, and its color
  static constexpr uint8_t DROP_NONE = 0, DROP_DOWN = 1, DROP_UP = 2;
  std::vector<uint8_t> drop_kind_;
  std::vector<uint32_t> drop_launch_;
  std::vector<ClockColor> drop_color_;
  // whether each LED was lit in the last frame, and the color it had then
  std::vector<uint8_t> lit_prev_;
  std::vector<ClockColor> lit_color_;
  bool drops_valid_ = false;
  uint32_t next_down_ = 0;
  uint32_t next_up_ = 0;
  // when a complete progress arc started to blink (0 = not blinking)
  uint32_t blink_since_ = 0;
  // the color of the light, as of this frame
  ClockColor light_hue_ = ClockColor(1, 1, 1);
  float default_track_ = 0.0f;
  ViewTransition default_transition_ = ViewTransition::WIPE;
  float default_transition_ms_ = DEFAULT_TRANSITION_MS;
  ClockView view_ = default_view_();
  ClockView prev_view_;
  struct {
    ViewTransition kind = ViewTransition::NONE;
    uint32_t start_time = 0;
    float duration = 0.0f;
  } transition_;
  // alerts breathe from the same phase, whenever they start
  uint32_t alert_start_ = 0;

  static ClockView default_view_() {
    ClockView view;
    view.show_clock = true;
    return view;
  }

  ClockPulse pulses_[MAX_PULSES];
  int pulse_head_ = 0;
  ClockRipple ripples_[MAX_RIPPLES];
  int ripple_head_ = 0;
  ClockSpin spins_[MAX_SPINS];
  int spin_head_ = 0;
};

inline ClockCompositor *clock_compositor = nullptr;

// Entry points of the API actions. Optional numbers <= 0 and empty strings mean the default.
// The transition of an API action: "" is the default transition, "none" none
inline ViewTransition clock_transition(const std::string &name) {
  if (name.empty() && clock_compositor != nullptr) return clock_compositor->default_transition();
  return view_transition_from_name(name);
}

inline void clock_trigger_pulse(std::vector<int> color_rgb, int count, float duration, float brightness) {
  if (clock_compositor != nullptr) clock_compositor->trigger_pulse(color_rgb, count, duration, brightness);
}

// position: clock position 0-59 (0 = 12 o'clock), < 0 = the second hand
inline void clock_trigger_ripple(float position, std::vector<int> color_rgb, float speed, float max_distance,
                                 float brightness) {
  if (clock_compositor != nullptr) clock_compositor->trigger_ripple(position, color_rgb, speed, max_distance, brightness);
}

// direction: "clockwise" or "counterclockwise", "" = clockwise
inline void clock_trigger_spin(std::vector<int> color_rgb, float rotations, float speed, const std::string &direction,
                               float brightness, float tail) {
  if (clock_compositor == nullptr) return;
  if (!direction.empty() && direction != "clockwise" && direction != "counterclockwise") {
    ESP_LOGW("clock", "Unknown spin direction '%s', using clockwise", direction.c_str());
  }
  clock_compositor->trigger_spin(color_rgb, rotations, speed, direction != "counterclockwise", brightness, tail);
}

// transition: "fade", "wipe" or "" for none; transition_ms <= 0 = default
inline void clock_show_clock(const std::string &transition, float transition_ms) {
  if (clock_compositor == nullptr) return;
  clock_compositor->show_clock_face(clock_transition(transition), transition_ms);
}

// sections: [start, end, r, g, b] per section, see ClockCompositor::show_progress()
inline void clock_show_progress(std::vector<float> sections, float track, bool show_clock, float brightness,
                                const std::string &transition, float transition_ms, const std::string &animation = "",
                                bool blink_when_complete = false) {
  if (clock_compositor == nullptr) return;
  clock_compositor->show_progress(sections, track, show_clock, brightness, clock_transition(transition),
                                  transition_ms, progress_animation_from_name(animation), blink_when_complete);
}

inline void clock_show_countdown(float duration_s, float remaining_s, std::vector<int> color_rgb, float track,
                                 bool show_clock, float brightness, const std::string &transition,
                                 float transition_ms, const std::string &animation = "") {
  if (clock_compositor == nullptr) return;
  clock_compositor->show_countdown(duration_s, remaining_s, color_rgb, track, show_clock, brightness,
                                   clock_transition(transition), transition_ms, progress_animation_from_name(animation));
}

inline void clock_show_alert(std::vector<int> color_rgb, float period_ms, bool show_clock, float brightness,
                             const std::string &transition, float transition_ms) {
  if (clock_compositor == nullptr) return;
  clock_compositor->show_alert(color_rgb, period_ms, show_clock, brightness, clock_transition(transition),
                               transition_ms);
}

// The "Clock View" select of Home Assistant: shows a view in the color of the light, so the views can be tried without service
// calls. Progress and countdown take their size from the number entities.
inline void clock_show_view_by_name(const std::string &name, float progress_percent, float countdown_minutes) {
  if (clock_compositor == nullptr) return;
  const ViewTransition transition = clock_compositor->default_transition();
  if (name == "Progress") {
    clock_compositor->show_progress({0.0f, esphome::clamp(progress_percent, 0.0f, 100.0f) / 100.0f, -1, -1, -1}, -1.0f,
                                    false, -1.0f, transition);
  } else if (name == "Countdown") {
    clock_compositor->show_countdown(std::max(countdown_minutes, 0.1f) * 60.0f, -1.0f, {}, -1.0f, false,
                                     -1.0f, transition);
  } else if (name == "Alert") {
    clock_compositor->show_alert({}, -1.0f, false, -1.0f, transition);
  } else {
    clock_compositor->show_clock_face(transition);
  }
}

inline std::string clock_view_name() {
  return clock_compositor == nullptr ? std::string("Clock") : std::string(clock_compositor->view_name());
}

// The "Clock View Transition" entities of Home Assistant
inline void clock_set_default_transition(const std::string &option, float transition_ms) {
  if (clock_compositor == nullptr) return;
  clock_compositor->set_default_transition(view_transition_from_option(option), transition_ms);
}

inline void clock_set_default_track(float percent) {
  if (clock_compositor == nullptr) return;
  clock_compositor->set_default_track(percent / 100.0f);
}

inline void clock_set_default_progress_animation(const std::string &option, float duration_ms) {
  if (clock_compositor == nullptr) return;
  clock_compositor->set_default_progress_animation(progress_animation_from_option(option), duration_ms);
}
