# ESPHome-Analog-Clock
ESPHome configuration example to create an animated clock using the Neopixel 60 LED ring.

[![Demo VIdeo](https://img.youtube.com/vi/RIAHLAqe6oY/0.jpg)](https://www.youtube.com/watch?v=RIAHLAqe6oY)

# Features

* [x] "Loading" animation while initializing
* [x] Works with (almost) any LED strip length
* [x] Fading animations for all hands when switching
* [x] Slowly fading tail behind the second hand
* [x] Overlapping (additive) hand colors
* [x] Settings
    * [x] Disable second hand
    * [x] Show hour indicators

## How to use

* Create a new ESPHome configuration YAML file
* Customize the basic device cnfiguration (Board, WiFi, etc.) to your liking
* Add the contents of [esphome_neopixel_clock_effect.yaml](./esphome_neopixel_clock_effect.yaml) to the file
* Customize the fields in the `substitutions` part at the top
* Deploy and enjoy!

# Contributing

GitHub is for social coding: if you want to write code, I encourage contributions through pull requests from forks
of this repository. Create GitHub tickets for bugs and new features and comment on the ones that you are interested in.

# License

CC0 see [LICENSE](./LICENSE)