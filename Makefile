.PHONY: test compile local-example sync-component

# Host-side unit tests of clock.h (no ESPHome needed)
test:
	@mkdir -p tests/build
	g++ -std=c++17 -fprofile-arcs -ftest-coverage -I tests tests/test_clock.cpp -o tests/build/clock_runner
	./tests/build/clock_runner
	@echo "\n--- Coverage Summary ---"
	@gcov -r tests/build/clock_runner-test_clock.gcno | grep -A 1 "clock.h" || true
	@rm -f *.gcov
	@rm -rf tests/build

# The example with the files of this checkout instead of GitHub, to try changes without pushing them
local-example:
	sed -e '/# remote$$/d' -e 's/^#local# //' examples/nodemcu.yaml > examples/_local.yaml

# Compile the example configuration
compile: local-example
	esphome compile examples/_local.yaml
	@# a misplaced section of the package is silently ignored by ESPHome, so check the boot code is really generated
	grep -q '^      clock_compositor = new ClockCompositor' examples/.esphome/build/analog-clock/src/main.cpp

# The virtual light is shared with ../esphome-configs (the LocalDeck uses it too), where it is developed.
sync-component:
	rsync -a --delete --exclude __pycache__ ../esphome-configs/custom_components/virtual_addressable_light/ components/virtual_addressable_light/
