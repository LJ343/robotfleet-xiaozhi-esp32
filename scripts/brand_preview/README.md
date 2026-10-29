# Real LVGL brand widget preview

Run from the firmware candidate root:

```sh
python3 scripts/brand_preview/run.py --output artifacts/brand-preview
python3 scripts/brand_preview/run.py --dark --output artifacts/brand-preview/dark
```

This is a host executable built with the candidate's own LVGL 9.5 checkout,
`robotfleet_widget.cc`, and `robotfleet_logo.cc`. It does not reimplement the
widget, animation, image conversion, or timer behavior. Its independent
`lv_conf.h` selects a software RGB565 display and never changes the firmware
configuration or managed components. CMake build products go under
`/tmp/robotfleet-brand-preview` by default. Dependencies are CMake, Ninja, a C/C++
compiler, Python 3, and Pillow.

The five `*-framebuffer.png` files retain the exact RGB565 framebuffer pixels
after conversion to RGB888 for PNG storage. The corresponding `idle.png`,
`listening.png`, `thinking.png`, `speaking.png`, and `offline.png` only add a
transparent mask outside the physical 360px circular panel. Idle and Offline
intentionally share the static logo; the retained foreground status layer
distinguishes these device states. These images contain the widget itself, not
a recreation of the firmware's status icons or subtitle layer.

The executable verifies the native 120px logo descriptor, position, scale and
rotation, then compares its opaque pixels throughout every animation. It samples
actual LVGL child rectangles every 50ms through 2200ms of each animated state to
check the circular clipping boundary and reserved foreground bands. The logo
bounds are `(120,104)` through `(239,223)`, a 25% linear reduction from the
hardware-confirmed first 160px version. The listening ring is 146–150px, its
halo is at most 158px, and the waiting arc is 150px. Visible animation objects stay between
`y=54` and `y=297`, leaving the top 54 rows and bottom 62 rows clear. The existing
LcdDisplay status and subtitle objects are independent foreground siblings;
their final font metrics and multiline content require integration and hardware
validation. This host test does not claim to measure those actual layers.

Lifecycle assertions inspect the actual LVGL timer list and paused flag. They
cover animated versus static states, all 49 transitions among seven states both
while visible and while hidden, hiding, state changes while hidden,
restoring visibility, destruction, and 100 repeated creation/destruction cycles.
The LVGL allocator must return to its exact initial free-byte count and pass
heap-integrity checks. Both widget and LVGL are compiled with AddressSanitizer
and UndefinedBehaviorSanitizer by default; `--no-sanitize` is available for hosts
without those runtimes. The LVGL allocator uses its own fixed pool, so its
integrity and free-byte checks complement sanitizer coverage.

`results.json` records the test result, source digests and scope; `test.log`
retains the executable's test output. The `--dark` capture uses the same black
background as the firmware's built-in dark theme, without altering logo pixels.
This workflow does not use a
serial port, start a server, build the complete ESP-IDF firmware, flash hardware,
or contact a cloud service.
