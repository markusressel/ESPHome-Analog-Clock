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
* Customize the basic device configuration (Board, Wi-Fi, etc.) to your liking
* Add the contents of [esphome_neopixel_clock_effect.yaml](./esphome_neopixel_clock_effect.yaml) to the file
* Add the `clock.h` file from [clock.h](./clock.h) to the `include` folder of your ESPHome configuration
* Customize the fields in the `substitutions` part at the top
* Deploy and enjoy!

## Watchface

### Paper Print

The provided [Watchface.svg](Watchface.svg) perfectly fits on the 60 LED NeoPixel ring.
You can print this, cut it out and simply glue/tape it to the ring to create the look visible in the video above. 

### 3D Printed Case

A simple 3D printed case can be found here:
https://cad.onshape.com/documents/0e006a8827f608cb658e942e/w/b4c3cdc5ddb2e7bb623cce6a/e/1f500e2bfa3036f79058a29e?renderMode=0&uiState=697d0fb9b6af62c58ba61274

<img src="images/3d_printed_case.jpg" width="300" alt="3D Printed Case"/>

# Contributing

GitHub is for social coding: if you want to write code, I encourage contributions through pull requests from forks
of this repository. Create GitHub tickets for bugs and new features and comment on the ones that you are interested in.

# License

CC0 see [LICENSE](./LICENSE)