# API

Everything the clock offers to Home Assistant: API actions and entities. The names below are those of ESPHome; in Home Assistant the
actions are `esphome.<device>_<action>` and the entities are prefixed with the device name.

## Concepts

* **Views** replace the plain clock face on the ring: `clock` (default), `progress`, `countdown` and `alert`. Showing a view of the kind that is
  shown already updates it in place (e.g. a progress that grows).
* Views, and the clock face itself, are only visible while the light runs its **"Clock" effect**. Turning the light on without an effect
  starts it; choosing another effect (rainbow, fireworks, ...) shows that effect instead.
* **Colors** are `[r, g, b]` (0-255). `[-1, -1, -1]` (or an empty list) means the color of the light, so a view follows the color picked in
  Home Assistant unless the call says otherwise.
* **Transitions** between views of different kinds: `"fade"`, `"wipe"`, `"none"`, or `""` for the default of the
  "Clock View Transition" entities.
* Parameters named *"Pass 0 for ..."* take 0 as "use the default", since ESPHome actions have no optional parameters.
* **Brightness** (0-1) scales the view with the brightness of the light. Pass 0 for full brightness.
* `show_clock` draws the clock hands over a view.

## Actions

### Views

| Action           | Parameters                                                                                                                  |
|------------------|-----------------------------------------------------------------------------------------------------------------------------|
| `show_clock`     | `transition`, `transition_ms`                                                                                               |
| `show_progress`  | `sections`, `track`, `show_clock`, `brightness`, `transition`, `transition_ms`, `animation`, `blink_when_complete`           |
| `show_countdown` | `duration_s`, `remaining_s`, `color_rgb`, `track`, `show_clock`, `brightness`, `transition`, `transition_ms`, `animation`    |
| `show_alert`     | `color_rgb`, `period_ms`, `show_clock`, `brightness`, `transition`, `transition_ms`                                          |

* `sections`: a flat list with `[start, end, r, g, b]` per section. `start` and `end` are fractions of the ring, clockwise from 12 o'clock
  (0-1). Later sections cover earlier ones. Example, a 40% print that is half way through its second phase: `[0, 0.2, 0, 200, 60, 0.2, 0.4, 255, 160, 0]`.
* `track`: the level (0-1) of white that marks the rest of the ring. Pass 0 for none and -1 for the "Clock View Track" entity.
* `animation`: how the view changes when it is shown again with other sections. `"none"`, `"fade"` (cross-fade), `"fill"` (the arc grows or
  shrinks smoothly), `"drop"` (new LEDs fall in from the end of the ring, removed ones float up) or `""` for the
  "Clock View Progress Animation" entities. A countdown can only `"drop"`.
* `blink_when_complete`: blink like a finished countdown once the whole ring is filled.
* `show_countdown`: the arc drains back to 12 o'clock. When the time is up, the ring blinks until another view is shown.
  `remaining_s` continues a countdown that started earlier (pass 0 for all of `duration_s`).
* `show_alert`: the whole ring breathes. `period_ms`: duration of one breath, pass 0 for 2000.

### Effects

Short effects on top of any view.

| Action           | Parameters                                                                                       |
|------------------|--------------------------------------------------------------------------------------------------|
| `trigger_pulse`  | `color_rgb`, `count`, `duration` (ms), `brightness`                                              |
| `trigger_ripple` | `position` (0-59, -1 for the second hand), `color_rgb`, `speed` (LEDs/s), `max_distance`, `brightness` |
| `trigger_spin`   | `color_rgb`, `rotations`, `speed`, `direction` (`"clockwise"`, `"counterclockwise"`), `brightness`, `tail` |
| `clear_effects`  | -                                                                                                |

## Entities

| Entity                                  | Type   | Persisted | Meaning                                                                                  |
|-----------------------------------------|--------|-----------|------------------------------------------------------------------------------------------|
| NeoPixel Light                          | light  | -         | The user facing light: color and brightness of the clock face and the views, effects     |
| Clock View                              | select | no        | The view that is shown (follows the actions); choose one to show it                      |
| Clock View Progress                     | number | yes       | The size (%) of the progress view that the select shows                                  |
| Clock View Countdown                    | number | yes       | The duration (min) of the countdown view that the select shows                           |
| Clock View Transition                   | select | yes       | Default transition: None, Fade, Wipe                                                     |
| Clock View Transition Duration          | number | yes       | Default duration of transitions (ms)                                                     |
| Clock View Progress Animation           | select | yes       | Default animation of progress changes: None, Fade, Fill, Drop                            |
| Clock View Progress Animation Duration  | number | yes       | Default duration of progress animations (ms)                                             |
| Clock View Track                        | number | yes       | Default level (%) of the white track                                                     |
| Countdown Remaining Time                | sensor | -         | Seconds left of the shown countdown, 0 when it is up, unknown while none is shown        |
| Clock Hand Style                        | select | yes       | Trail, Jump, Smooth, Comet                                                               |
| Clock Seconds                           | switch | yes       | Show the second hand                                                                     |
| Hour Indicators                         | switch | yes       | Show a marker at every hour                                                              |
| Wide Hour Hand                          | switch | yes       | Draw the hour hand three LEDs wide                                                         |

**Persisted entities and firmware updates:** on the ESP8266 the saved values are found by the order in which the entities are created.
Adding or removing a persisted entity in front of others resets their saved values once.

## From Python (haad)

The `AnalogClock` device class of [haad](https://github.com/markusressel/haad) wraps these actions and entities.
Besides calling the actions it has a `ClockViewScheduler`: sources *claim* the ring with a view, a priority (ambient, normal, high,
critical) and a condition, and the scheduler shows the view of the highest claim, rotates between equal ones and returns to the clock
when no claim is active.
