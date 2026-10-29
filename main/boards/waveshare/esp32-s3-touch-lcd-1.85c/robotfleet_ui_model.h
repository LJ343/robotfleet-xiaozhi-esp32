#ifndef ROBOTFLEET_UI_MODEL_H
#define ROBOTFLEET_UI_MODEL_H

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace robotfleet {

enum class Activity { Idle, Connecting, Listening, Thinking, Speaking, Offline, Other };

inline constexpr int kLogoSize = 120;
inline constexpr int kRingMinDiameter = 146;
inline constexpr int kRingMaxDiameter = 150;
inline constexpr int kHaloExpansion = 8;
inline constexpr int kArcDiameter = 150;

struct Layout {
    int center_x;
    int center_y;
    int logo_size;
    int wave_center_y;
};

// The native logo is never scaled or rotated by LVGL. Keep the animation clear of
// the original top status and bottom subtitle layers on this 360px round panel.
inline Layout MakeLayout(int width, int height) {
    return {width / 2, height / 2 - 16, kLogoSize, height / 2 + 98};
}

inline bool IsAnimated(Activity activity) {
    return activity == Activity::Connecting || activity == Activity::Listening ||
           activity == Activity::Thinking || activity == Activity::Speaking;
}

struct AnimationFrame {
    int ring_diameter = kRingMinDiameter;
    uint8_t ring_opacity = 0;
    uint8_t halo_opacity = 0;
    int arc_rotation = 0;
    std::array<int, 7> wave_heights{};
};

inline AnimationFrame FrameAt(Activity activity, uint32_t elapsed_ms) {
    constexpr float kTau = 6.2831853071795864769f;
    AnimationFrame frame;
    if (activity == Activity::Listening) {
        const float phase = static_cast<float>(elapsed_ms % 2200) / 2200.0f;
        const float breath = (1.0f - std::cos(kTau * phase)) * 0.5f;
        frame.ring_diameter =
            kRingMinDiameter + static_cast<int>((kRingMaxDiameter - kRingMinDiameter) * breath);
        frame.ring_opacity = static_cast<uint8_t>(90.0f + 110.0f * breath);
        frame.halo_opacity = static_cast<uint8_t>(10.0f + 26.0f * breath);
    } else if (activity == Activity::Connecting || activity == Activity::Thinking) {
        frame.arc_rotation = static_cast<int>((elapsed_ms % 1400) * 360 / 1400);
    } else if (activity == Activity::Speaking) {
        // This is an activity illustration, independent of microphone/PCM data.
        // Fixed objects and bounded math keep work out of the audio path.
        const float phase = kTau * static_cast<float>(elapsed_ms % 1200) / 1200.0f;
        for (std::size_t i = 0; i < frame.wave_heights.size(); ++i) {
            const float envelope = 1.0f - 0.13f * std::abs(static_cast<int>(i) - 3);
            const float wave = (std::sin(phase + static_cast<float>(i) * 0.8f) + 1.0f) * 0.5f;
            frame.wave_heights[i] = 4 + static_cast<int>(20.0f * envelope * wave);
        }
    }
    return frame;
}

}  // namespace robotfleet

#endif  // ROBOTFLEET_UI_MODEL_H
