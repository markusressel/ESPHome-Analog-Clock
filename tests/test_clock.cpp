#include "esphome.h"

namespace esphome {
uint32_t g_mock_millis = 0;
}

#include "../components/analog_clock/clock.h"

#include <cassert>
#include <cmath>
#include <iostream>

#define ASSERT_NEAR(a, b, err) assert(std::fabs((a) - (b)) < (err))

static constexpr int NUM_LEDS = 60;

// A clock like examples/nodemcu.yaml: 60 LEDs, the "Clock" effect running on the user facing light
struct ClockFixture {
  AddressableLight physical{NUM_LEDS};
  AddressableLight virtual_main{NUM_LEDS};
  LightState main_state;
  Switch seconds{true};
  Switch indicators{false};
  Switch wide_hour{false};
  ClockCompositor comp;

  explicit ClockFixture(float max_brightness = 1.0f, int offset = 0)
      : comp(&physical, &virtual_main, &main_state, &seconds, &indicators, &wide_hour, offset, max_brightness) {
    g_mock_millis = 1000;
    main_state.remote_values.state = 1.0f;
    main_state.remote_values.br = 1.0f;
  }

  // Runs frames of the clock effect and the compositor for `ms` milliseconds
  void run(uint32_t ms, int hour, int minute, int second, bool clock_effect = true) {
    uint32_t end = g_mock_millis + ms;
    do {
      g_mock_millis += ClockCompositor::MIN_FRAME_TIME_MS;
      if (clock_effect) comp.mark_clock_effect();
      comp.loop(true, hour, minute, second);
    } while (g_mock_millis < end);
  }

  Color out(int i) { return physical.get_led(i).c; }
  bool lit(int i) {
    Color c = out(i);
    return c.r > 0 || c.g > 0 || c.b > 0;
  }
  int lit_count() {
    int n = 0;
    for (int i = 0; i < NUM_LEDS; i++) n += lit(i) ? 1 : 0;
    return n;
  }
};

void test_encode_output() {
  ClockFixture f(0.5f);
  // black stays black
  assert(f.comp.encode_output(ClockColor()) == Color(0, 0, 0, 0));
  // full brightness hits the limit: 50% perceived like `color_correct: [50%, 50%, 50%]` did before
  Color full = f.comp.encode_output(ClockColor(1, 0, 0));
  assert(full.r == (uint8_t) (std::pow(0.5f, 2.8f) * 255.0f + 0.5f));
  assert(full.g == 0 && full.b == 0);
  // a lit pixel never turns off, however dim
  Color dim = f.comp.encode_output(ClockColor(ClockCompositor::to_linear(0.01f), 0, 0));
  assert(dim.r >= 1);
  // the hue is kept
  Color mixed = f.comp.encode_output(ClockColor(1, 0.25f, 0));
  ASSERT_NEAR(mixed.g / (float) mixed.r, 0.25f, 0.05f);
  std::cout << "test_encode_output passed." << std::endl;
}

// With `color_correct: [14%, 14%, 14%]` on the physical light, bytes below 7 round to 0 on the strip
void test_min_output_keeps_dim_pixels_lit() {
  AddressableLight physical{NUM_LEDS};
  AddressableLight virtual_main{NUM_LEDS};
  LightState main_state;
  Switch off{false};
  ClockCompositor comp(&physical, &virtual_main, &main_state, &off, &off, &off, 0, 1.0f, 7);
  Color dim = comp.encode_output(ClockColor(ClockCompositor::to_linear(0.002f), 0, 0));
  assert(dim.r == 7);
  Color full = comp.encode_output(ClockColor(1, 0, 0));
  assert(full.r == 255);
  std::cout << "test_min_output_keeps_dim_pixels_lit passed." << std::endl;
}

void test_jump_hand_positions() {
  ClockFixture f;
  f.comp.set_hand_style(HandStyle::JUMP);
  f.run(100, 3, 15, 30);
  assert(f.comp.shows_clock());
  // the hour hand moves one LED every 12 minutes: 3 * 5 + 1
  assert(f.out(16).b > 0 && f.out(16).r == 0 && f.out(16).g == 0);
  assert(f.out(15).g > 0 && f.out(15).b == 0);
  assert(f.out(30).r > 0);
  assert(f.lit_count() == 3);
  std::cout << "test_jump_hand_positions passed." << std::endl;
}

void test_overlapping_hands_add_up() {
  ClockFixture f;
  f.comp.set_hand_style(HandStyle::JUMP);
  // 12:00:00, all hands on LED 0
  f.run(100, 0, 0, 0);
  Color c = f.out(0);
  assert(c.r > 0 && c.g > 0 && c.b > 0);
  assert(f.lit_count() == 1);
  std::cout << "test_overlapping_hands_add_up passed." << std::endl;
}

void test_wide_hour_hand_and_seconds_switch() {
  ClockFixture f;
  f.comp.set_hand_style(HandStyle::JUMP);
  f.wide_hour.state = true;
  f.seconds.state = false;
  f.run(100, 6, 0, 45);
  assert(f.out(29).b > 0 && f.out(30).b > 0 && f.out(31).b > 0);
  assert(!f.lit(45));
  std::cout << "test_wide_hour_hand_and_seconds_switch passed." << std::endl;
}

void test_indicators_are_covered_by_hands() {
  ClockFixture f;
  f.comp.set_hand_style(HandStyle::JUMP);
  f.indicators.state = true;
  f.seconds.state = false;
  f.run(100, 3, 15, 0);
  // indicators every 5 LEDs, white
  Color indicator = f.out(5);
  assert(indicator.r > 0 && indicator.r == indicator.g && indicator.g == indicator.b);
  assert(!f.lit(6));
  // the minute hand on an indicator shows only its own color
  assert(f.out(15).g > 0 && f.out(15).r == 0 && f.out(15).b == 0);
  std::cout << "test_indicators_are_covered_by_hands passed." << std::endl;
}

void test_trail_fades_out() {
  ClockFixture f;
  f.comp.set_hand_style(HandStyle::TRAIL);
  f.run(1000, 0, 30, 10);
  uint8_t head = f.out(10).r;
  f.run(500, 0, 30, 11);
  // the old position fades, the new one is at full strength
  assert(f.out(10).r > 0 && f.out(10).r < head);
  assert(f.out(11).r == head);
  f.run(10000, 0, 30, 11);
  assert(!f.lit(10));
  std::cout << "test_trail_fades_out passed." << std::endl;
}

void test_jump_has_no_trail() {
  ClockFixture f;
  f.comp.set_hand_style(HandStyle::JUMP);
  f.run(1000, 0, 30, 10);
  f.run(40, 0, 30, 11);
  assert(!f.lit(10));
  std::cout << "test_jump_has_no_trail passed." << std::endl;
}

void test_smooth_second_hand_glides() {
  ClockFixture f;
  f.comp.set_hand_style(HandStyle::SMOOTH);
  f.seconds.state = true;
  f.run(40, 0, 30, 19);
  // the second starts now: at its middle, the hand is halfway between LED 20 and 21
  f.run(500, 0, 30, 20);
  ASSERT_NEAR(f.out(21).r / (float) f.out(20).r, 1.0f, 0.15f);
  assert(f.out(20).r > 0);
  f.run(400, 0, 30, 20);
  // later in the second, it is mostly on the next LED
  assert(f.out(21).r > f.out(20).r);
  std::cout << "test_smooth_second_hand_glides passed." << std::endl;
}

void test_comet_tail() {
  ClockFixture f;
  f.comp.set_hand_style(HandStyle::COMET);
  f.run(40, 0, 0, 39);
  f.run(20, 0, 0, 40);
  // the tail fades out behind the head and does not reach ahead of it
  assert(f.out(40).r > f.out(37).r);
  assert(f.out(37).r > f.out(34).r);
  assert(f.lit(34));
  assert(!f.out(42).r);
  std::cout << "test_comet_tail passed." << std::endl;
}

void test_offset_rotates_the_face() {
  ClockFixture f(1.0f, 30);
  f.comp.set_hand_style(HandStyle::JUMP);
  f.seconds.state = false;
  f.run(100, 0, 10, 0);
  // the minute hand at 10 minutes is LED 10, shifted by 30
  assert(f.out(40).g > 0);
  assert(f.out(10).g == 0);
  std::cout << "test_offset_rotates_the_face passed." << std::endl;
}

void test_other_effects_show_the_virtual_light() {
  ClockFixture f;
  f.run(100, 0, 0, 0);
  assert(f.comp.shows_clock());
  // another effect: the "Clock" effect stops, the buffer of the virtual light shows
  f.virtual_main.get_led(5).c = Color(255, 0, 0);
  f.run(400, 0, 0, 0, false);
  assert(!f.comp.shows_clock());
  assert(f.out(5).r > 0);
  assert(!f.lit(0));
  std::cout << "test_other_effects_show_the_virtual_light passed." << std::endl;
}

void test_clock_fades_out_when_turned_off() {
  ClockFixture f;
  f.comp.set_hand_style(HandStyle::JUMP);
  f.run(100, 0, 20, 0);
  uint8_t full = f.out(20).g;
  // ESPHome stops the effect when the light turns off; the clock stays and fades out
  f.main_state.remote_values.state = 0.0f;
  f.run(500, 0, 20, 0, false);
  assert(f.comp.shows_clock());
  assert(f.out(20).g > 0 && f.out(20).g < full);
  f.run(600, 0, 20, 0, false);
  assert(!f.lit(20));
  std::cout << "test_clock_fades_out_when_turned_off passed." << std::endl;
}

void test_turning_on_without_effect_shows_the_virtual_light() {
  ClockFixture f;
  f.run(100, 0, 20, 0);
  f.main_state.remote_values.state = 0.0f;
  f.run(2000, 0, 20, 0, false);
  assert(f.comp.shows_clock());
  // turned on with a plain color: the light fades in with it, not with the clock
  f.virtual_main.get_led(5).c = Color(255, 0, 0);
  f.main_state.remote_values.state = 1.0f;
  f.run(400, 0, 20, 0, false);
  assert(!f.comp.shows_clock());
  assert(f.lit(5) && !f.lit(20));
  std::cout << "test_turning_on_without_effect_shows_the_virtual_light passed." << std::endl;
}

void test_brightness_changes_smoothly() {
  ClockFixture f;
  f.comp.set_hand_style(HandStyle::JUMP);
  f.run(100, 0, 20, 0);
  uint8_t full = f.out(20).g;
  f.main_state.remote_values.br = 0.2f;
  f.run(300, 0, 20, 0);
  uint8_t between = f.out(20).g;
  f.run(1000, 0, 20, 0);
  uint8_t dim = f.out(20).g;
  assert(full > between && between > dim && dim >= 1);
  std::cout << "test_brightness_changes_smoothly passed." << std::endl;
}

void test_pulse() {
  ClockFixture f;
  f.comp.set_hand_style(HandStyle::JUMP);
  f.seconds.state = false;
  f.run(100, 0, 20, 0);
  f.comp.trigger_pulse({0, 0, 255}, 2, 1000.0f);
  f.run(500, 0, 20, 0);
  // mid pulse, the whole ring is lit in the color on top of the clock
  assert(f.lit_count() == NUM_LEDS);
  assert(f.out(7).b > 0 && f.out(7).r == 0);
  assert(f.out(20).g > 0 && f.out(20).b > 0);
  // between the pulses, the ring is (almost) dark
  f.run(500, 0, 20, 0);
  assert(f.out(7).b <= 1);
  f.run(1100, 0, 20, 0);
  assert(!f.comp.has_active_effects());
  // only the hour (LED 1) and minute hand are left
  assert(f.lit_count() == 2);
  std::cout << "test_pulse passed." << std::endl;
}

void test_effects_show_while_the_light_is_off() {
  ClockFixture f;
  f.main_state.remote_values.state = 0.0f;
  f.run(2000, 0, 20, 0, false);
  assert(f.lit_count() == 0);
  f.comp.trigger_pulse({}, 1, 1000.0f);
  f.run(500, 0, 20, 0, false);
  Color c = f.out(3);
  assert(c.r > 0 && c.r == c.g && c.g == c.b);
  std::cout << "test_effects_show_while_the_light_is_off passed." << std::endl;
}

void test_ripple_spreads_both_ways() {
  ClockFixture f;
  f.main_state.remote_values.state = 0.0f;
  f.run(2000, 0, 0, 0, false);
  // from 3 o'clock at 20 LEDs per second
  f.comp.trigger_ripple(15.0f, {255, 0, 0}, 20.0f);
  f.run(500, 0, 0, 0, false);
  // after 0.5s the ring is 10 LEDs away in both directions
  assert(f.out(25).r > 0 && f.out(5).r > 0);
  assert(!f.lit(15));
  ASSERT_NEAR((float) f.out(25).r, (float) f.out(5).r, 1.0f);
  f.run(2000, 0, 0, 0, false);
  assert(!f.comp.has_active_effects());
  std::cout << "test_ripple_spreads_both_ways passed." << std::endl;
}

void test_spin() {
  ClockFixture f;
  f.main_state.remote_values.state = 0.0f;
  f.run(2000, 0, 0, 0, false);
  // one rotation per second, clockwise from 12 o'clock
  f.comp.trigger_spin({0, 255, 0}, 1.0f, 1.0f, true, 1.0f, 5.0f);
  f.run(250, 0, 0, 0, false);
  // the head is around LED 15, the tail behind it, nothing ahead
  assert(f.out(14).g > 0 && f.out(12).g > 0);
  assert(f.out(14).g > f.out(11).g);
  assert(!f.lit(17));
  assert(!f.lit(45));
  f.run(1200, 0, 0, 0, false);
  assert(!f.comp.has_active_effects());
  assert(f.lit_count() == 0);
  std::cout << "test_spin passed." << std::endl;
}

void test_spin_counterclockwise_and_rainbow() {
  ClockFixture f;
  f.main_state.remote_values.state = 0.0f;
  f.run(2000, 0, 0, 0, false);
  clock_compositor = &f.comp;
  clock_trigger_spin({}, 1.0f, 1.0f, "counterclockwise", 1.0f, 5.0f);
  f.run(250, 0, 0, 0, false);
  // counterclockwise from 12 o'clock: around LED 45
  assert(f.lit(46) && f.lit(48));
  assert(!f.lit(14));
  // rainbow: the color depends on the position (45 of 60 is in the blue to magenta range)
  assert(f.out(46).b > 0);
  clock_compositor = nullptr;
  std::cout << "test_spin_counterclockwise_and_rainbow passed." << std::endl;
}

void test_spin_covers_the_clock() {
  ClockFixture f;
  f.comp.set_hand_style(HandStyle::JUMP);
  f.seconds.state = false;
  f.run(100, 0, 15, 0);
  f.comp.trigger_spin({255, 0, 0}, 1.0f, 1.0f, true, 1.0f, 3.0f);
  f.run(240, 0, 15, 0);
  // the comet's head over the minute hand replaces its color
  assert(f.out(14).r > 0 && f.out(14).g == 0);
  std::cout << "test_spin_covers_the_clock passed." << std::endl;
}

void test_clear_effects() {
  ClockFixture f;
  f.comp.trigger_pulse();
  f.comp.trigger_ripple();
  f.comp.trigger_spin();
  assert(f.comp.has_active_effects());
  f.comp.clear_effects();
  assert(!f.comp.has_active_effects());
  std::cout << "test_clear_effects passed." << std::endl;
}

void test_hand_style_names() {
  assert(hand_style_from_name("Trail") == HandStyle::TRAIL);
  assert(hand_style_from_name("Jump") == HandStyle::JUMP);
  assert(hand_style_from_name("Smooth") == HandStyle::SMOOTH);
  assert(hand_style_from_name("Comet") == HandStyle::COMET);
  assert(hand_style_from_name("unknown") == HandStyle::TRAIL);
  std::cout << "test_hand_style_names passed." << std::endl;
}


// A clock without distractions: hands jump, no seconds, 12:00
static void calm(ClockFixture &f) {
  f.comp.set_hand_style(HandStyle::JUMP);
  f.seconds.state = false;
  f.run(100, 6, 30, 0);
}

void test_progress_fills_sections_and_blends_the_edge() {
  ClockFixture f;
  calm(f);
  assert(f.comp.view_kind() == ViewKind::CLOCK);
  // green for the first half, red from 0.75 on, the edge of the green one is in the middle of LED 15
  float edge = 15.5f / 60.0f;
  f.comp.show_progress({0.0f, edge, 0, 255, 0, 0.75f, 1.0f, 255, 0, 0});
  f.run(100, 6, 30, 0);
  assert(f.comp.view_kind() == ViewKind::PROGRESS);
  assert(f.out(0).g > 0 && f.out(0).r == 0);
  assert(f.out(14).g > 0);
  assert(f.out(15).g > 0 && f.out(15).g < f.out(14).g);
  assert(!f.lit(16) && !f.lit(44));
  assert(f.out(45).r > 0 && f.out(59).r > 0 && f.out(45).g == 0);
  // the clock hands are not drawn
  assert(f.out(30).b == 0);
  std::cout << "test_progress_fills_sections_and_blends_the_edge passed." << std::endl;
}

void test_progress_track_and_brightness() {
  ClockFixture f;
  calm(f);
  f.comp.show_progress({0.0f, 0.5f, 0, 255, 0}, 0.2f, false, 0.5f);
  f.run(100, 6, 30, 0);
  // the rest of the ring is a dim white
  assert(f.lit(40));
  assert(f.out(40).r == f.out(40).g && f.out(40).g == f.out(40).b);
  assert(f.out(40).g < f.out(5).g);
  // brightness 0.5: dimmer than a full brightness section
  ClockFixture full;
  calm(full);
  full.comp.show_progress({0.0f, 0.5f, 0, 255, 0});
  full.run(100, 6, 30, 0);
  assert(f.out(5).g < full.out(5).g);
  std::cout << "test_progress_track_and_brightness passed." << std::endl;
}

void test_progress_can_show_the_hands_over_it() {
  ClockFixture f;
  calm(f);
  f.comp.show_progress({0.0f, 1.0f, 0, 255, 0}, 0.0f, true);
  f.run(100, 6, 30, 0);
  // the hour hand (blue) at LED 32 covers the arc
  assert(f.out(32).b > 0 && f.out(32).g == 0);
  assert(f.out(10).g > 0 && f.out(10).b == 0);
  std::cout << "test_progress_can_show_the_hands_over_it passed." << std::endl;
}

void test_showing_the_same_kind_updates_in_place() {
  ClockFixture f;
  calm(f);
  f.comp.show_progress({0.0f, 0.25f, 0, 255, 0});
  f.run(100, 6, 30, 0);
  assert(!f.lit(20));
  f.comp.show_progress({0.0f, 0.5f, 0, 255, 0}, 0.0f, false, -1.0f, ViewTransition::WIPE, 1000.0f);
  f.run(20, 6, 30, 0);
  // no transition between views of the same kind
  assert(!f.comp.in_transition());
  assert(f.lit(20));
  std::cout << "test_showing_the_same_kind_updates_in_place passed." << std::endl;
}

void test_wipe_sweeps_clockwise() {
  ClockFixture f;
  calm(f);
  f.comp.show_progress({0.0f, 1.0f, 0, 255, 0}, 0.0f, false, -1.0f, ViewTransition::WIPE, 1000.0f);
  f.run(20, 6, 30, 0);
  assert(f.comp.in_transition());
  f.run(480, 6, 30, 0);
  // halfway: the new view covers the first half of the ring (the second half is still the clock)
  assert(f.out(10).g > 0 && f.out(20).g > 0);
  assert(!f.lit(50));
  f.run(600, 6, 30, 0);
  assert(!f.comp.in_transition());
  assert(f.out(50).g > 0);
  std::cout << "test_wipe_sweeps_clockwise passed." << std::endl;
}

void test_fade_blends_both_views() {
  ClockFixture f;
  calm(f);
  f.comp.show_progress({0.0f, 1.0f, 0, 255, 0}, 0.0f, false, -1.0f, ViewTransition::FADE, 1000.0f);
  f.run(20, 6, 30, 0);
  f.run(480, 6, 30, 0);
  Color mid = f.out(10);
  assert(mid.g > 0);
  f.run(600, 6, 30, 0);
  // the new view's pixel is brighter than halfway through the fade
  assert(f.out(10).g > mid.g);
  std::cout << "test_fade_blends_both_views passed." << std::endl;
}

void test_countdown_drains_and_blinks_when_the_time_is_up() {
  ClockFixture f;
  calm(f);
  assert(std::isnan(f.comp.countdown_remaining_s()));
  f.comp.show_countdown(10.0f, -1.0f, {0, 0, 255});
  f.run(100, 6, 30, 0);
  ASSERT_NEAR(f.comp.countdown_remaining_s(), 9.9f, 0.1f);
  assert(f.comp.view_kind() == ViewKind::COUNTDOWN);
  // everything is left
  assert(f.lit(0) && f.lit(30) && f.lit(59));
  f.run(5000, 6, 30, 0);
  // halfway: the arc ends around LED 30
  assert(f.lit(10) && f.lit(25));
  assert(!f.lit(35) && !f.lit(55));
  f.run(5100, 6, 30, 0);
  assert(f.comp.countdown_remaining_s() == 0.0f);
  // time is up: the view stays, and the ring blinks (lit in the first half of a period, dark in the second)
  assert(f.comp.view_kind() == ViewKind::COUNTDOWN);
  assert(!f.comp.has_active_effects());
  bool lit_seen = false, dark_seen = false;
  for (int i = 0; i < 100; i++) {
    f.run(20, 6, 30, 0);
    bool all_lit = f.lit(0) && f.lit(30) && f.lit(59);
    lit_seen = lit_seen || all_lit;
    dark_seen = dark_seen || f.lit_count() == 0;
  }
  assert(lit_seen && dark_seen);
  f.run(60000, 6, 30, 0);
  assert(f.comp.view_kind() == ViewKind::COUNTDOWN);
  // until another view is shown
  f.comp.show_clock_face();
  f.run(100, 6, 30, 0);
  assert(f.lit_count() == 2);
  std::cout << "test_countdown_drains_and_blinks_when_the_time_is_up passed." << std::endl;
}

void test_countdown_continues_with_the_remaining_time() {
  ClockFixture f;
  calm(f);
  f.comp.show_countdown(100.0f, 25.0f);
  f.run(100, 6, 30, 0);
  // a quarter is left: the arc covers LEDs 0-14
  assert(f.lit(10) && f.lit(14));
  assert(!f.lit(17));
  std::cout << "test_countdown_continues_with_the_remaining_time passed." << std::endl;
}

void test_alert_breathes() {
  ClockFixture f;
  calm(f);
  f.comp.show_alert({255, 0, 0}, 2000.0f);
  uint8_t lowest = 255, highest = 0;
  for (int i = 0; i < 40; i++) {
    f.run(50, 6, 30, 0);
    lowest = std::min(lowest, f.out(20).r);
    highest = std::max(highest, f.out(20).r);
    assert(f.out(20).g == 0 && f.out(20).b == 0);
  }
  assert(lowest >= 1);
  assert(highest > lowest * 4);
  // the whole ring breathes together
  assert(f.out(0).r == f.out(45).r);
  std::cout << "test_alert_breathes passed." << std::endl;
}

void test_clock_face_returns() {
  ClockFixture f;
  calm(f);
  f.comp.show_alert();
  f.run(100, 6, 30, 0);
  f.comp.show_clock_face(ViewTransition::FADE, 500.0f);
  f.run(700, 6, 30, 0);
  assert(f.comp.view_kind() == ViewKind::CLOCK);
  assert(f.lit_count() == 2 && f.out(32).b > 0);
  std::cout << "test_clock_face_returns passed." << std::endl;
}

void test_view_api_entry_points() {
  ClockFixture f;
  calm(f);
  clock_compositor = &f.comp;
  clock_show_progress({0.0f, 0.5f, 255, 0, 0}, 0.0f, false, 0.0f, "wipe", 0.0f);
  assert(f.comp.view_kind() == ViewKind::PROGRESS && f.comp.in_transition());
  clock_show_countdown(30.0f, 0.0f, {-1, -1, -1}, 0.0f, false, 0.0f, "none", 0.0f);
  assert(f.comp.view_kind() == ViewKind::COUNTDOWN && !f.comp.in_transition());
  clock_show_alert({}, 0.0f, true, 0.0f, "fade", 0.0f);
  assert(f.comp.view_kind() == ViewKind::ALERT);
  clock_show_clock("", 0.0f);
  assert(f.comp.view_kind() == ViewKind::CLOCK);
  assert(view_transition_from_name("unknown") == ViewTransition::NONE);
  clock_compositor = nullptr;
  clock_show_clock("", 0.0f);
  clock_show_progress({}, 0.0f, false, 0.0f, "", 0.0f);
  clock_show_countdown(1.0f, 0.0f, {}, 0.0f, false, 0.0f, "", 0.0f);
  clock_show_alert({}, 0.0f, false, 0.0f, "", 0.0f);
  std::cout << "test_view_api_entry_points passed." << std::endl;
}

void test_view_by_name_for_the_home_assistant_select() {
  ClockFixture f;
  calm(f);
  assert(clock_view_name() == "Clock");
  clock_compositor = &f.comp;
  clock_set_default_track(8.0f);
  clock_show_view_by_name("Progress", 50.0f, 5.0f);
  assert(f.comp.view_kind() == ViewKind::PROGRESS && clock_view_name() == "Progress");
  f.run(1000, 6, 30, 0);
  // half of the ring
  assert(f.out(10).g > f.out(35).g && f.out(25).g > f.out(35).g && f.out(35).g == f.out(35).r);
  clock_show_view_by_name("Countdown", 0.0f, 2.0f);
  assert(clock_view_name() == "Countdown");
  clock_show_view_by_name("Alert", 0.0f, 0.0f);
  assert(clock_view_name() == "Alert");
  clock_show_view_by_name("Clock", 0.0f, 0.0f);
  assert(clock_view_name() == "Clock");
  clock_compositor = nullptr;
  assert(clock_view_name() == "Clock");
  clock_show_view_by_name("Alert", 0.0f, 0.0f);
  std::cout << "test_view_by_name_for_the_home_assistant_select passed." << std::endl;
}

void test_default_transition() {
  ClockFixture f;
  calm(f);
  clock_compositor = &f.comp;
  // "" is the default, which is a wipe until changed
  clock_show_alert({}, 0.0f, false, 0.0f, "", 0.0f);
  assert(f.comp.in_transition());
  f.run(700, 6, 30, 0);
  assert(!f.comp.in_transition());
  clock_set_default_transition("None", 0.0f);
  clock_show_clock("", 0.0f);
  assert(!f.comp.in_transition());
  clock_set_default_transition("Fade", 2000.0f);
  clock_show_alert({}, 0.0f, false, 0.0f, "", 0.0f);
  assert(f.comp.in_transition());
  // the default duration is 2 s now
  f.run(1000, 6, 30, 0);
  assert(f.comp.in_transition());
  f.run(1200, 6, 30, 0);
  assert(!f.comp.in_transition());
  // "none" overrides the default
  clock_show_clock("none", 0.0f);
  assert(!f.comp.in_transition());
  clock_compositor = nullptr;
  std::cout << "test_default_transition passed." << std::endl;
}

void test_default_track_follows_the_entity() {
  ClockFixture f;
  calm(f);
  f.comp.show_progress({0.0f, 0.5f, 0, 255, 0});
  f.run(100, 6, 30, 0);
  // no track by default
  assert(!f.lit(40));
  clock_compositor = &f.comp;
  clock_set_default_track(20.0f);
  f.run(100, 6, 30, 0);
  // the shown view follows
  assert(f.lit(40));
  // an explicit track wins, 0 is none
  f.comp.show_progress({0.0f, 0.5f, 0, 255, 0}, 0.0f);
  f.run(100, 6, 30, 0);
  assert(!f.lit(40));
  clock_compositor = nullptr;
  clock_set_default_track(20.0f);
  std::cout << "test_default_track_follows_the_entity passed." << std::endl;
}

// A progress view of a green bar up to `fraction` of the ring, with the given animation
static void bar(ClockFixture &f, float fraction, ProgressAnimation animation) {
  f.comp.show_progress({0.0f, fraction, 0, 255, 0}, 0.0f, false, -1.0f, ViewTransition::NONE, -1.0f, animation);
}

void test_progress_without_animation_jumps() {
  ClockFixture f;
  calm(f);
  bar(f, 0.25f, ProgressAnimation::NONE);
  f.run(100, 6, 30, 0);
  bar(f, 0.5f, ProgressAnimation::NONE);
  f.run(20, 6, 30, 0);
  assert(f.lit(29) && !f.lit(31));
  std::cout << "test_progress_without_animation_jumps passed." << std::endl;
}

void test_progress_fill_moves_the_edge() {
  ClockFixture f;
  calm(f);
  f.comp.set_default_progress_animation(ProgressAnimation::FILL, 1000.0f);
  bar(f, 0.25f, ProgressAnimation::FROM_ENTITY);
  f.run(100, 6, 30, 0);
  assert(f.lit(10) && !f.lit(20));
  bar(f, 0.5f, ProgressAnimation::FROM_ENTITY);
  f.run(520, 6, 30, 0);
  // halfway (eased: exactly in the middle): the edge is around LED 22
  assert(f.lit(20) && !f.lit(26));
  f.run(800, 6, 30, 0);
  assert(f.lit(29) && !f.lit(32));
  // shrinking works too
  bar(f, 0.25f, ProgressAnimation::FROM_ENTITY);
  f.run(520, 6, 30, 0);
  assert(f.lit(20) && !f.lit(26));
  f.run(800, 6, 30, 0);
  assert(!f.lit(20));
  std::cout << "test_progress_fill_moves_the_edge passed." << std::endl;
}

void test_progress_fill_with_other_sections_fades() {
  ClockFixture f;
  calm(f);
  bar(f, 0.25f, ProgressAnimation::FILL);
  f.run(100, 6, 30, 0);
  // a second section: no pairs to move, so it fades in
  f.comp.show_progress({0.0f, 0.25f, 0, 255, 0, 0.5f, 0.75f, 255, 0, 0}, 0.0f, false, -1.0f, ViewTransition::NONE, -1.0f,
                       ProgressAnimation::FILL);
  f.run(20, 6, 30, 0);
  assert(!f.lit(40) || f.out(40).r < 5);
  f.run(1000, 6, 30, 0);
  assert(f.out(40).r > 0);
  std::cout << "test_progress_fill_with_other_sections_fades passed." << std::endl;
}

void test_progress_fade() {
  ClockFixture f;
  calm(f);
  bar(f, 0.25f, ProgressAnimation::FADE);
  f.run(100, 6, 30, 0);
  bar(f, 0.5f, ProgressAnimation::FADE);
  f.run(20, 6, 30, 0);
  uint8_t early = f.out(25).g;
  f.run(400, 6, 30, 0);
  uint8_t mid = f.out(25).g;
  f.run(1000, 6, 30, 0);
  uint8_t late = f.out(25).g;
  assert(early < mid && mid < late);
  std::cout << "test_progress_fade passed." << std::endl;
}

void test_progress_drop_falls_in_from_the_end_of_the_ring() {
  ClockFixture f;
  calm(f);
  f.comp.set_default_progress_animation(ProgressAnimation::DROP, 1000.0f);
  bar(f, 0.25f, ProgressAnimation::DROP);
  f.run(100, 6, 30, 0);
  bar(f, 0.3f, ProgressAnimation::DROP);
  f.run(20, 6, 30, 0);
  // three new LEDs (15-17): a drop is on its way and has not landed yet
  assert(!f.lit(17));
  bool drop_seen = false, walked_over_the_arc = false;
  for (int i = 0; i < 80; i++) {
    f.run(20, 6, 30, 0);
    // the drop has the color of the arc (green): it is seen beyond the new LEDs, and never moves over the old arc
    for (int j = 20; j < 60; j++) drop_seen = drop_seen || f.out(j).g > 0;
    walked_over_the_arc = walked_over_the_arc || f.out(7).g != f.out(8).g;
  }
  assert(drop_seen && !walked_over_the_arc);
  f.run(2000, 6, 30, 0);
  assert(f.lit(17) && !f.lit(20));
  std::cout << "test_progress_drop_falls_in_from_the_end_of_the_ring passed." << std::endl;
}

void test_progress_drop_floats_up_when_the_value_decreases() {
  ClockFixture f;
  calm(f);
  f.comp.set_default_progress_animation(ProgressAnimation::DROP, 1000.0f);
  bar(f, 0.3f, ProgressAnimation::DROP);
  f.run(100, 6, 30, 0);
  bar(f, 0.1f, ProgressAnimation::DROP);
  f.run(20, 6, 30, 0);
  // nothing jumped: the removed LEDs are still there until their drop lifts off, the highest first
  assert(f.lit(10));
  bool floating_seen = false, walked_over_the_arc = false;
  for (int i = 0; i < 80; i++) {
    f.run(20, 6, 30, 0);
    for (int j = 20; j < 60; j++) floating_seen = floating_seen || f.out(j).g > 0;
    walked_over_the_arc = walked_over_the_arc || f.out(1).g != f.out(2).g;
  }
  assert(floating_seen && !walked_over_the_arc);
  f.run(2000, 6, 30, 0);
  assert(f.lit(5) && !f.lit(10));
  std::cout << "test_progress_drop_floats_up_when_the_value_decreases passed." << std::endl;
}

void test_progress_drop_stays_still_when_the_lit_leds_stay_the_same() {
  ClockFixture f;
  calm(f);
  f.comp.set_default_progress_animation(ProgressAnimation::DROP, 1000.0f);
  // 18.6 and 19.2 LEDs: LED 18 is lit in both, LED 19 is not
  f.comp.show_progress({0.0f, 18.6f / 60.0f, 0, 255, 0}, 0.0f);
  f.run(100, 6, 30, 0);
  bar(f, 19.2f / 60.0f, ProgressAnimation::DROP);
  bool drop_seen = false;
  for (int i = 0; i < 60; i++) {
    f.run(20, 6, 30, 0);
    // neither a drop nor a fading edge: the LEDs look the same all the time
    for (int j = 19; j < 60; j++) drop_seen = drop_seen || f.lit(j);
    assert(f.out(18).g == f.out(5).g);
  }
  assert(!drop_seen);
  // 18.4 to 18.6 LEDs: LED 18 switches on, with a drop (not a fade)
  f.comp.show_progress({0.0f, 18.4f / 60.0f, 0, 255, 0}, 0.0f);
  f.run(1500, 6, 30, 0);
  assert(!f.lit(18));
  bar(f, 18.6f / 60.0f, ProgressAnimation::DROP);
  f.run(20, 6, 30, 0);
  assert(!f.lit(18));
  bool falling = false;
  for (int i = 0; i < 60; i++) {
    f.run(20, 6, 30, 0);
    for (int j = 19; j < 60; j++) falling = falling || f.lit(j);
  }
  assert(falling && f.lit(18));
  std::cout << "test_progress_drop_stays_still_when_the_lit_leds_stay_the_same passed." << std::endl;
}

void test_progress_drop_changes_do_not_affect_drops_in_flight() {
  ClockFixture f;
  calm(f);
  f.comp.set_default_progress_animation(ProgressAnimation::DROP, 1000.0f);
  bar(f, 0.25f, ProgressAnimation::DROP);
  f.run(100, 6, 30, 0);
  bar(f, 0.5f, ProgressAnimation::DROP);
  // the first drop (for LED 15) is about at LED 47 after 200 ms
  f.run(200, 6, 30, 0);
  bool drop_in_flight = false;
  for (int j = 44; j < 51; j++) drop_in_flight = drop_in_flight || f.lit(j);
  assert(drop_in_flight);
  // the value drops again before the drops have landed: their LEDs are not wanted any more, so the drops are gone at
  // once (no ghost where they were)
  bar(f, 0.25f, ProgressAnimation::DROP);
  f.run(20, 6, 30, 0);
  for (int j = 20; j < 60; j++) assert(!f.lit(j));
  f.run(3000, 6, 30, 0);
  assert(f.lit_count() == 15 && f.lit(14) && !f.lit(15));
  // a smaller change while drops are on their way: they keep falling and land
  bar(f, 0.5f, ProgressAnimation::DROP);
  f.run(200, 6, 30, 0);
  bar(f, 0.45f, ProgressAnimation::DROP);
  f.run(3000, 6, 30, 0);
  assert(f.lit(26) && !f.lit(27) && f.lit_count() == 27);
  std::cout << "test_progress_drop_changes_do_not_affect_drops_in_flight passed." << std::endl;
}

void test_countdown_shares_the_settings_of_the_progress() {
  ClockFixture f;
  calm(f);
  f.comp.set_default_track(0.0f);
  // the default animation is a drop: the arc is made of whole LEDs, which float up when they go out
  f.comp.set_default_progress_animation(ProgressAnimation::DROP, 1000.0f);
  f.comp.show_countdown(60.0f, -1.0f, {0, 0, 255}, 0.0f);
  f.run(100, 6, 30, 0);
  assert(f.lit(0) && f.lit(59));
  // after 2.5 s (of 60), two and a half LEDs are gone: the edge LED (57) is off from the half on
  f.run(2400, 6, 30, 0);
  assert(f.lit(55) && !f.lit(58));
  // the LED that just went out lifts off and floats up to the end of the ring: a blue spot above the arc
  bool floating = false;
  for (int i = 0; i < 60; i++) {
    f.run(20, 6, 30, 0);
    for (int j = 58; j < 60; j++) floating = floating || f.out(j).b > 0;
  }
  assert(floating);
  // without drops a countdown has no whole LEDs and no animation: the edge is smooth
  ClockFixture g;
  calm(g);
  g.comp.show_countdown(60.0f, 30.0f, {0, 0, 255}, 0.0f, false, -1.0f, ViewTransition::NONE, -1.0f,
                        ProgressAnimation::FILL);
  g.run(100, 6, 30, 0);
  uint8_t edge = 0;
  for (int i = 0; i < 60; i++) {
    uint8_t b = g.out(i).b;
    if (b > 0 && b < g.out(0).b) edge = b;
  }
  assert(edge > 0);
  std::cout << "test_countdown_shares_the_settings_of_the_progress passed." << std::endl;
}

void test_progress_blinks_when_complete_if_asked() {
  ClockFixture f;
  calm(f);
  f.comp.show_progress({0.0f, 1.0f, 0, 255, 0}, 0.0f, false, -1.0f, ViewTransition::NONE, -1.0f, ProgressAnimation::NONE,
                       true);
  bool lit_seen = false, dark_seen = false;
  for (int i = 0; i < 100; i++) {
    f.run(20, 6, 30, 0);
    lit_seen = lit_seen || f.lit_count() == 60;
    dark_seen = dark_seen || f.lit_count() == 0;
  }
  assert(lit_seen && dark_seen);
  // it stops when the value goes down
  f.comp.show_progress({0.0f, 0.5f, 0, 255, 0}, 0.0f, false, -1.0f, ViewTransition::NONE, -1.0f,
                       ProgressAnimation::NONE, true);
  f.run(1500, 6, 30, 0);
  assert(f.lit_count() == 30);
  // without the option the full ring just stays
  ClockFixture g;
  calm(g);
  g.comp.show_progress({0.0f, 1.0f, 0, 255, 0}, 0.0f);
  for (int i = 0; i < 100; i++) {
    g.run(20, 6, 30, 0);
    assert(g.lit_count() == 60);
  }
  // with drops, the blink starts when the last drop has landed
  ClockFixture h;
  calm(h);
  h.comp.set_default_progress_animation(ProgressAnimation::DROP, 1000.0f);
  h.comp.show_progress({0.0f, 0.5f, 0, 255, 0}, 0.0f, false, -1.0f, ViewTransition::NONE, -1.0f,
                       ProgressAnimation::DROP, true);
  h.run(100, 6, 30, 0);
  h.comp.show_progress({0.0f, 1.0f, 0, 255, 0}, 0.0f, false, -1.0f, ViewTransition::NONE, -1.0f,
                       ProgressAnimation::DROP, true);
  h.run(300, 6, 30, 0);
  // still falling, not blinking: the first half is steady
  assert(h.lit(10) && h.lit(25));
  h.run(5000, 6, 30, 0);
  bool dark = false;
  for (int i = 0; i < 100; i++) {
    h.run(20, 6, 30, 0);
    dark = dark || h.lit_count() == 0;
  }
  assert(dark);
  std::cout << "test_progress_blinks_when_complete_if_asked passed." << std::endl;
}

void test_views_take_the_color_of_the_light() {
  ClockFixture f;
  calm(f);
  f.main_state.remote_values.c = Color(0, 0, 255);
  // no color: the light's (blue), for a progress section, a countdown and an alert
  f.comp.show_progress({0.0f, 0.5f, -1, -1, -1}, 0.0f);
  f.run(100, 6, 30, 0);
  assert(f.out(10).b > 0 && f.out(10).r == 0 && f.out(10).g == 0);
  f.comp.show_countdown(60.0f, -1.0f, {}, 0.0f);
  f.run(100, 6, 30, 0);
  assert(f.out(10).b > 0 && f.out(10).r == 0 && f.out(10).g == 0);
  f.comp.show_alert();
  f.run(1000, 6, 30, 0);
  assert(f.out(10).b > 0 && f.out(10).r == 0 && f.out(10).g == 0);
  // an explicit color wins
  f.comp.show_progress({0.0f, 0.5f, 255, 0, 0}, 0.0f);
  f.run(100, 6, 30, 0);
  assert(f.out(10).r > 0 && f.out(10).b == 0);
  // and the color follows the light while the view is shown
  f.comp.show_progress({0.0f, 0.5f, -1, -1, -1}, 0.0f);
  f.run(100, 6, 30, 0);
  f.main_state.remote_values.c = Color(0, 255, 0);
  f.run(100, 6, 30, 0);
  assert(f.out(10).g > 0 && f.out(10).b == 0);
  // the home assistant select uses it, too
  clock_compositor = &f.comp;
  clock_show_view_by_name("Progress", 50.0f, 5.0f);
  f.run(1000, 6, 30, 0);
  assert(f.out(10).g > 0 && f.out(10).r == 0);
  clock_compositor = nullptr;
  std::cout << "test_views_take_the_color_of_the_light passed." << std::endl;
}

// Whether LED 59 was lit while the arc lost or gained the single LED `led` (a drop of `travel_ms` for the whole ring)
static bool end_reached(int led, bool gained, float travel_ms) {
  ClockFixture f;
  calm(f);
  f.comp.set_default_progress_animation(ProgressAnimation::DROP, travel_ms);
  bar(f, (led + (gained ? 0 : 1)) / 60.0f, ProgressAnimation::DROP);
  f.run(100, 6, 30, 0);
  bar(f, (led + (gained ? 1 : 0)) / 60.0f, ProgressAnimation::DROP);
  for (int i = 0; i < 100; i++) {
    f.run(20, 6, 30, 0);
    if (f.lit(59)) return true;
  }
  return false;
}

void test_drops_reach_the_ends_of_the_ring_however_fast_they_are() {
  // every single drop touches the last LED: floating up ends there, and falling starts there. Frames are 20 ms apart, so
  // a fast drop moves several LEDs per frame and must not hop over it.
  for (float travel : {300.0f, 500.0f, 700.0f, 800.0f, 900.0f, 1100.0f}) {
    for (int led = 5; led < 50; led++) {
      assert(end_reached(led, false, travel));
      assert(end_reached(led, true, travel));
    }
  }
  std::cout << "test_drops_reach_the_ends_of_the_ring_however_fast_they_are passed." << std::endl;
}

void test_progress_animation_needs_a_displayed_view() {
  ClockFixture f;
  calm(f);
  bar(f, 0.25f, ProgressAnimation::FILL);
  // the light is off: nothing is rendered, so there is nothing to animate from
  f.main_state.remote_values.state = 0.0f;
  f.run(2000, 6, 30, 0, false);
  bar(f, 0.5f, ProgressAnimation::FILL);
  f.run(20, 6, 30, 0);
  f.main_state.remote_values.state = 1.0f;
  f.run(1500, 6, 30, 0);
  assert(f.lit(29));
  // equal sections change nothing, and the entity names work
  bar(f, 0.5f, ProgressAnimation::FILL);
  clock_compositor = &f.comp;
  clock_set_default_progress_animation("Drop", 500.0f);
  clock_show_progress({0.0f, 0.6f, 0, 255, 0}, 0.0f, false, 0.0f, "none", 0.0f, "fade");
  assert(progress_animation_from_name("drop") == ProgressAnimation::DROP);
  assert(progress_animation_from_name("none") == ProgressAnimation::NONE);
  assert(progress_animation_from_name("") == ProgressAnimation::FROM_ENTITY);
  assert(progress_animation_from_option("Fill") == ProgressAnimation::FILL);
  assert(progress_animation_from_option("None") == ProgressAnimation::NONE);
  clock_compositor = nullptr;
  clock_set_default_progress_animation("Drop", 500.0f);
  std::cout << "test_progress_animation_needs_a_displayed_view passed." << std::endl;
}

int main() {
  test_encode_output();
  test_min_output_keeps_dim_pixels_lit();
  test_jump_hand_positions();
  test_overlapping_hands_add_up();
  test_wide_hour_hand_and_seconds_switch();
  test_indicators_are_covered_by_hands();
  test_trail_fades_out();
  test_jump_has_no_trail();
  test_smooth_second_hand_glides();
  test_comet_tail();
  test_offset_rotates_the_face();
  test_other_effects_show_the_virtual_light();
  test_clock_fades_out_when_turned_off();
  test_turning_on_without_effect_shows_the_virtual_light();
  test_brightness_changes_smoothly();
  test_pulse();
  test_effects_show_while_the_light_is_off();
  test_ripple_spreads_both_ways();
  test_spin();
  test_spin_counterclockwise_and_rainbow();
  test_spin_covers_the_clock();
  test_clear_effects();
  test_hand_style_names();
  test_progress_fills_sections_and_blends_the_edge();
  test_progress_track_and_brightness();
  test_progress_can_show_the_hands_over_it();
  test_showing_the_same_kind_updates_in_place();
  test_wipe_sweeps_clockwise();
  test_fade_blends_both_views();
  test_countdown_drains_and_blinks_when_the_time_is_up();
  test_countdown_continues_with_the_remaining_time();
  test_alert_breathes();
  test_clock_face_returns();
  test_view_api_entry_points();
  test_view_by_name_for_the_home_assistant_select();
  test_default_transition();
  test_default_track_follows_the_entity();
  test_progress_without_animation_jumps();
  test_progress_fill_moves_the_edge();
  test_progress_fill_with_other_sections_fades();
  test_progress_fade();
  test_progress_drop_falls_in_from_the_end_of_the_ring();
  test_progress_drop_floats_up_when_the_value_decreases();
  test_progress_drop_stays_still_when_the_lit_leds_stay_the_same();
  test_progress_drop_changes_do_not_affect_drops_in_flight();
  test_countdown_shares_the_settings_of_the_progress();
  test_progress_blinks_when_complete_if_asked();
  test_views_take_the_color_of_the_light();
  test_drops_reach_the_ends_of_the_ring_however_fast_they_are();
  test_progress_animation_needs_a_displayed_view();
  std::cout << "All clock tests passed." << std::endl;
  return 0;
}
