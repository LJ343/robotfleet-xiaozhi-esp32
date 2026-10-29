#include "robotfleet_logo.h"
#include "robotfleet_widget.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
constexpr int kWidth = 360;
constexpr int kHeight = 360;
constexpr int kExpectedLogoSize = 120;
constexpr int kExpectedLogoX = 120;
constexpr int kExpectedLogoY = 104;
std::array<uint16_t, kWidth * kHeight> framebuffer{};
std::array<uint16_t, kWidth * kHeight> draw_buffer{};

void Require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void Flush(lv_display_t* display, const lv_area_t* area, uint8_t* pixels) {
    const auto* source = reinterpret_cast<const uint16_t*>(pixels);
    for (int y = area->y1; y <= area->y2; ++y) {
        for (int x = area->x1; x <= area->x2; ++x) {
            Require(x >= 0 && x < kWidth && y >= 0 && y < kHeight,
                    "flush rectangle outside framebuffer");
            framebuffer[y * kWidth + x] = *source++;
        }
    }
    lv_display_flush_ready(display);
}

void Advance(uint32_t milliseconds) {
    for (uint32_t elapsed = 0; elapsed < milliseconds; elapsed += 10) {
        lv_tick_inc(10);
        lv_timer_handler();
    }
    lv_refr_now(nullptr);
}

uint64_t PixelHash(int x1 = 0, int y1 = 0, int x2 = 359, int y2 = 359) {
    uint64_t hash = 1469598103934665603ULL;
    for (int y = y1; y <= y2; ++y) {
        for (int x = x1; x <= x2; ++x) {
            hash = (hash ^ framebuffer[y * kWidth + x]) * 1099511628211ULL;
        }
    }
    return hash;
}

uint64_t OpaqueLogoPixelHash() {
    // Transparent corners of the image rectangle may contain the surrounding
    // animation. Only opaque pixels belong to the logo itself.
    const auto& image = robotfleet::kRobotFleetLogo;
    Require(image.header.cf == LV_COLOR_FORMAT_ARGB8888, "unexpected device logo format");
    uint64_t hash = 1469598103934665603ULL;
    for (int y = 0; y < kExpectedLogoSize; ++y) {
        for (int x = 0; x < kExpectedLogoSize; ++x) {
            if (image.data[y * image.header.stride + x * 4 + 3] == 255) {
                hash = (hash ^ framebuffer[(y + kExpectedLogoY) * kWidth + x + kExpectedLogoX]) *
                       1099511628211ULL;
            }
        }
    }
    return hash;
}

size_t TimerCount() {
    size_t count = 0;
    for (auto* timer = lv_timer_get_next(nullptr); timer != nullptr;
         timer = lv_timer_get_next(timer)) {
        ++count;
    }
    return count;
}

lv_timer_t* WidgetTimer(const robotfleet::RobotFleetWidget* widget) {
    lv_timer_t* found = nullptr;
    for (auto* timer = lv_timer_get_next(nullptr); timer != nullptr;
         timer = lv_timer_get_next(timer)) {
        if (lv_timer_get_user_data(timer) == widget) {
            Require(found == nullptr, "widget unexpectedly owns multiple timers");
            found = timer;
        }
    }
    return found;
}

void CheckTimer(const robotfleet::RobotFleetWidget& widget, bool running) {
    auto* timer = WidgetTimer(&widget);
    Require(timer != nullptr, "actual LVGL widget timer missing");
    Require(lv_timer_get_paused(timer) == !running, "actual LVGL timer pause state wrong");
    Require(widget.animation_running() == running, "animation_running disagrees with LVGL");
}

void SaveFrame(const std::filesystem::path& path) {
    std::ofstream file(path, std::ios::binary);
    Require(file.good(), "cannot write framebuffer");
    file << "P6\n" << kWidth << " " << kHeight << "\n255\n";
    for (const auto pixel : framebuffer) {
        const uint8_t red = ((pixel >> 11) & 0x1f) * 255 / 31;
        const uint8_t green = ((pixel >> 5) & 0x3f) * 255 / 63;
        const uint8_t blue = (pixel & 0x1f) * 255 / 31;
        const char rgb[] = {static_cast<char>(red), static_cast<char>(green),
                            static_cast<char>(blue)};
        file.write(rgb, sizeof(rgb));
    }
}

lv_obj_t* FindImage(lv_obj_t* object) {
    if (lv_obj_check_type(object, &lv_image_class)) {
        return object;
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(object); ++i) {
        auto* found = FindImage(lv_obj_get_child(object, i));
        if (found != nullptr) {
            return found;
        }
    }
    return nullptr;
}

void CheckGeometry(const robotfleet::RobotFleetWidget& widget) {
    lv_obj_update_layout(widget.root());
    auto* logo = FindImage(widget.root());
    Require(logo != nullptr, "real widget logo image missing");
    Require(lv_image_get_src(logo) == &robotfleet::kRobotFleetLogo,
            "widget does not reference the device logo descriptor");
    Require(
        lv_obj_get_width(logo) == kExpectedLogoSize && lv_obj_get_height(logo) == kExpectedLogoSize,
        "logo must be rendered at native 120px");
    Require(robotfleet::kRobotFleetLogo.header.w == kExpectedLogoSize &&
                robotfleet::kRobotFleetLogo.header.h == kExpectedLogoSize,
            "device descriptor must contain the native 120px asset");
    Require(lv_image_get_scale_x(logo) == 256 && lv_image_get_scale_y(logo) == 256 &&
                lv_image_get_rotation(logo) == 0,
            "logo must remain unscaled and unrotated");
    lv_area_t coordinates;
    lv_obj_get_coords(logo, &coordinates);
    Require(coordinates.x1 == 120 && coordinates.x2 == 239 && coordinates.y1 == 104 &&
                coordinates.y2 == 223,
            "logo position differs from approved 360px layout");
    for (uint32_t i = 0; i < lv_obj_get_child_count(widget.root()); ++i) {
        auto* child = lv_obj_get_child(widget.root(), i);
        if (lv_obj_has_flag(child, LV_OBJ_FLAG_HIDDEN)) {
            continue;
        }
        lv_obj_get_coords(child, &coordinates);
        if (child != logo) {
            const int width = lv_obj_get_width(child);
            const int height = lv_obj_get_height(child);
            if (lv_obj_check_type(child, &lv_arc_class)) {
                Require(width == 150 && height == 150, "waiting arc was not reduced to 150px");
            } else if (width > 4) {
                Require(width == height && width >= 146 && width <= 158,
                        "listening ring/halo does not match reduced logo proportions");
            } else {
                Require(width == 4 && height >= 4 && height <= 24,
                        "speaking waveform dimensions changed unexpectedly");
                Require(coordinates.y1 + height / 2 == 278,
                        "speaking waveform center moved unexpectedly");
            }
        }
        Require(coordinates.y1 >= 54 && coordinates.y2 <= 297,
                "animation or logo intrudes into reserved top/bottom foreground bands");
        for (int x : {coordinates.x1, coordinates.x2}) {
            for (int y : {coordinates.y1, coordinates.y2}) {
                const int dx = x - 180;
                const int dy = y - 180;
                Require(dx * dx + dy * dy < 180 * 180,
                        "visible logo/animation rectangle touches round-screen clipping");
            }
        }
    }
}

void CheckStaticState(robotfleet::RobotFleetWidget& widget, robotfleet::Activity state) {
    widget.SetActivity(state);
    Advance(100);
    CheckTimer(widget, false);
    const auto before = PixelHash();
    Advance(750);
    Require(PixelHash() == before, "static state changed pixels while timer paused");
}

void CheckAnimatedState(robotfleet::RobotFleetWidget& widget, robotfleet::Activity state) {
    widget.SetActivity(state);
    Advance(100);
    CheckTimer(widget, true);
    const auto before = PixelHash();
    const auto logo_before = OpaqueLogoPixelHash();
    Advance(350);
    Require(PixelHash() != before, "active timer did not update rendered pixels");
    Require(OpaqueLogoPixelHash() == logo_before,
            "activity animation altered opaque pixels of the native logo");
    CheckGeometry(widget);
    // Sample a full breathing cycle, longer than the arc and waveform periods.
    for (int elapsed = 0; elapsed < 2200; elapsed += 50) {
        Advance(50);
        CheckGeometry(widget);
        Require(OpaqueLogoPixelHash() == logo_before,
                "full animation cycle altered opaque native logo pixels");
    }
}

void CheckTransitionMatrix(robotfleet::RobotFleetWidget& widget) {
    using robotfleet::Activity;
    const std::array<std::pair<Activity, bool>, 7> states = {{
        {Activity::Idle, false},
        {Activity::Connecting, true},
        {Activity::Listening, true},
        {Activity::Thinking, true},
        {Activity::Speaking, true},
        {Activity::Offline, false},
        {Activity::Other, false},
    }};
    for (const auto& [from, from_animated] : states) {
        for (const auto& [to, to_animated] : states) {
            widget.SetActivity(from);
            CheckTimer(widget, from_animated);
            widget.SetActivity(to);
            CheckTimer(widget, to_animated);
            Advance(10);
            CheckGeometry(widget);
            widget.SetVisible(false);
            widget.SetActivity(from);
            CheckTimer(widget, false);
            widget.SetActivity(to);
            CheckTimer(widget, false);
            widget.SetVisible(true);
            CheckTimer(widget, to_animated);
        }
    }
}

void WriteResults(const std::filesystem::path& output, size_t baseline_timers, size_t initial_free,
                  size_t final_free, bool dark) {
    std::ofstream file(output / "results.json");
    file << "{\n"
         << "  \"lvgl_version\": \"" << LVGL_VERSION_MAJOR << "." << LVGL_VERSION_MINOR << "."
         << LVGL_VERSION_PATCH << "\",\n"
         << "  \"source\": \"device robotfleet_widget.cc + robotfleet_logo.cc\",\n"
         << "  \"test_result\": \"PASS\",\n"
         << "  \"theme_background\": \"" << (dark ? "#000000" : "#ffffff") << "\",\n"
         << "  \"logo_bounds\": [120, 104, 239, 223],\n"
         << "  \"native_logo_size\": [120, 120],\n"
         << "  \"surrounding_geometry\": {\"ring_diameter_range\": [146, 150], "
            "\"halo_max_diameter\": 158, \"arc_diameter\": 150, \"wave_center_y\": 278},\n"
         << "  \"reserved_foreground_bands\": {\"top\": [0, 53], \"bottom\": [298, 359]},\n"
         << "  \"timers_before_and_after_destruction\": [" << baseline_timers << ", "
         << baseline_timers << "],\n"
         << "  \"repeat_lifecycle_cycles\": 100,\n"
         << "  \"state_transition_matrix\": {\"states\": 7, \"visible_cases\": 49, "
            "\"hidden_cases\": 49},\n"
         << "  \"geometry_sampling\": {\"period_ms\": 50, \"duration_per_animated_state_ms\": "
            "2200},\n"
         << "  \"lvgl_free_bytes_before_after_cycles\": [" << initial_free << ", " << final_free
         << "],\n"
         << "  \"checks\": [\"all animated states change framebuffer pixels\", "
            "\"idle/offline/other remain static\", \"native logo pixels do not animate\", "
            "\"actual LVGL timer pauses while hidden\", \"hidden state changes stay paused\", "
            "\"show restores active state\", \"show preserves static state pause\", "
            "\"destruction removes timer and object tree\", \"100 cycles retain no timers or "
            "objects\", "
            "\"LVGL heap integrity\", \"animation stays inside circular screen and reserved "
            "bands\"],\n"
         << "  \"scope\": \"Real widget rendering and lifecycle; complete LcdDisplay status, "
            "subtitles, "
            "ESP-IDF integration and physical LCD are not simulated.\"\n"
         << "}\n";
}
}  // namespace

int main(int argc, char** argv) {
    try {
        Require(argc == 2 || argc == 3, "usage: brand_preview OUTPUT_DIRECTORY [dark]");
        const bool dark = argc == 3 && std::string(argv[2]) == "dark";
        const std::filesystem::path output(argv[1]);
        std::filesystem::create_directories(output);
        lv_init();
        auto* display = lv_display_create(kWidth, kHeight);
        Require(display != nullptr, "LVGL display allocation failed");
        lv_display_set_color_format(display, LV_COLOR_FORMAT_RGB565);
        lv_display_set_buffers(display, draw_buffer.data(), nullptr, sizeof(draw_buffer),
                               LV_DISPLAY_RENDER_MODE_FULL);
        lv_display_set_flush_cb(display, Flush);
        auto* screen = lv_screen_active();
        lv_obj_remove_style_all(screen);
        lv_obj_set_style_bg_color(screen, lv_color_hex(dark ? 0x000000 : 0xffffff), 0);
        lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
        lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
        Advance(100);
        const auto baseline_timers = TimerCount();
        const auto baseline_children = lv_obj_get_child_count(screen);
        {
            robotfleet::RobotFleetWidget widget(screen, kWidth, kHeight);
            Require(TimerCount() == baseline_timers + 1, "widget must own one real timer");
            CheckStaticState(widget, robotfleet::Activity::Idle);
            CheckStaticState(widget, robotfleet::Activity::Offline);
            CheckStaticState(widget, robotfleet::Activity::Other);
            for (auto state : {robotfleet::Activity::Connecting, robotfleet::Activity::Listening,
                               robotfleet::Activity::Thinking, robotfleet::Activity::Speaking}) {
                CheckAnimatedState(widget, state);
            }
            CheckTransitionMatrix(widget);
            widget.SetVisible(false);
            CheckTimer(widget, false);
            Require(lv_obj_has_flag(widget.root(), LV_OBJ_FLAG_HIDDEN), "hide did not hide root");
            widget.SetActivity(robotfleet::Activity::Listening);
            Advance(500);
            CheckTimer(widget, false);
            widget.SetVisible(true);
            CheckTimer(widget, true);
            widget.SetVisible(false);
            widget.SetActivity(robotfleet::Activity::Offline);
            widget.SetVisible(true);
            CheckTimer(widget, false);
            const std::array<std::pair<const char*, robotfleet::Activity>, 5> captures = {{
                {"idle", robotfleet::Activity::Idle},
                {"listening", robotfleet::Activity::Listening},
                {"thinking", robotfleet::Activity::Thinking},
                {"speaking", robotfleet::Activity::Speaking},
                {"offline", robotfleet::Activity::Offline},
            }};
            for (const auto& [name, state] : captures) {
                widget.SetActivity(state);
                Advance(550);
                CheckGeometry(widget);
                SaveFrame(output / (std::string(name) + ".ppm"));
            }
        }
        Advance(100);
        Require(TimerCount() == baseline_timers, "destructor leaked real LVGL timer");
        Require(lv_obj_get_child_count(screen) == baseline_children, "destructor leaked root tree");
        lv_mem_monitor_t initial_memory;
        lv_mem_monitor(&initial_memory);
        for (int i = 0; i < 100; ++i) {
            {
                robotfleet::RobotFleetWidget widget(screen, kWidth, kHeight);
                widget.SetActivity(robotfleet::Activity::Speaking);
                Advance(50);
                widget.SetVisible(false);
                widget.SetActivity(robotfleet::Activity::Thinking);
                widget.SetVisible(true);
                Advance(50);
            }
            Advance(10);
            Require(TimerCount() == baseline_timers, "repeated lifecycle leaked timer");
            Require(lv_obj_get_child_count(screen) == baseline_children,
                    "repeated lifecycle leaked objects");
            Require(lv_mem_test() == LV_RESULT_OK, "LVGL heap integrity failure");
        }
        lv_mem_monitor_t final_memory;
        lv_mem_monitor(&final_memory);
        Require(final_memory.free_size == initial_memory.free_size,
                "repeated lifecycle leaked LVGL heap");
        WriteResults(output, baseline_timers, initial_memory.free_size, final_memory.free_size,
                     dark);
        lv_display_delete(display);
        lv_deinit();
        std::cout << "PASS: true LVGL RGB565 rendering, native logo, safe geometry, timers, 100 "
                     "lifecycle cycles\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << "\n";
        return 1;
    }
}
