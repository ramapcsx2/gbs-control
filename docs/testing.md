# Testing

## Quick start

```bash
# run all tests
pio test -e native

# run a single test group
pio test -e native -f test_wr

# run tests + generate coverage report
pio test -e native && \
  xcrun llvm-profdata merge -sparse default.profraw -o default.profdata && \
  xcrun llvm-cov report .pio/build/native/program \
    -instr-profile=default.profdata \
    -ignore-filename-regex='(Unity|unity_config)'
```

## Architecture

Tests run on the host machine (macOS/Linux) using PlatformIO's `native` environment with the [Unity](https://github.com/ThrowTheSwitch/Unity) test framework. No ESP8266 hardware needed.

**How it works:** Pure-logic functions are extracted from `gbs-control.ino` into standalone headers under `include/`. These headers have zero Arduino/ESP8266 dependencies — just standard C (`<stdint.h>`, `<string.h>`, `<stdlib.h>`). Test files include them directly.

```
include/
  wr_queue.h      # WrEntry struct, ring buffer helpers, isKnownRegister()
  wr_parse.h      # parseUint() — strtoul + validation wrapper
  moving_avg.h    # Moving average filter (extracted from getMovingAverage)
  shift_calc.h    # Horizontal shift calculations
  scale_calc.h    # Horizontal/vertical scale calculations
  timing_calc.h   # Timing register calculations (blanking, vtotal)
  color_calc.h    # Sharpness toggle calculations
  led_calc.h      # LED sync status state machine

test/
  test_wr/        # Ring buffer, register validation, parsing
  test_scale/     # Horizontal/vertical scaling logic
  test_shift/     # Horizontal shift with wrap-around
  test_timing/    # Blanking register calculations, vtotal
  test_color/     # Sharpness toggle state machine
  test_led/       # LED state from sync counters
  test_utils/     # Moving average, integer parsing
  coverage_flags.py  # SCons script to add profiling to linker
```

The `[env:native]` in `platformio.ini` uses `test_build_src = false` so that `gbs-control.ino` (with its ESP8266 dependencies) is never compiled for tests.

## Adding tests

1. Extract pure logic from `gbs-control.ino` into a header under `include/`
2. Create a new directory `test/test_<name>/` with a `test_<name>.cpp` file
3. Each test file needs `#include <unity.h>` and a `main()` that calls `UNITY_BEGIN()` / `RUN_TEST()` / `UNITY_END()`

## Code coverage

Coverage uses Apple Clang's LLVM instrumentation (`-fprofile-instr-generate -fcoverage-mapping`), not GCC's gcov. The `--coverage` / gcov approach doesn't work on macOS ARM64 because Apple Clang doesn't ship the gcov runtime.

### Generate text summary

```bash
pio test -e native
xcrun llvm-profdata merge -sparse default.profraw -o default.profdata
xcrun llvm-cov report .pio/build/native/program \
  -instr-profile=default.profdata \
  -ignore-filename-regex='(Unity|unity_config)'
```

### Generate HTML report

```bash
xcrun llvm-cov show .pio/build/native/program \
  -instr-profile=default.profdata \
  --format=html -output-dir=coverage/ \
  -ignore-filename-regex='(Unity|unity_config)'
```

Open `coverage/index.html` in a browser.

### Coverage artifacts

`*.profraw`, `*.profdata`, and `coverage/` are in `.gitignore`.

## Build verification

Always verify that the firmware still compiles after modifying headers:

```bash
pio run -e d1_mini
```
