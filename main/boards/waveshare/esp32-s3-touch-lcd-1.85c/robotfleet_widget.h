#ifndef ROBOTFLEET_WIDGET_H
#define ROBOTFLEET_WIDGET_H

#include "robotfleet_ui_model.h"

#include <lvgl.h>

namespace robotfleet {

// All methods, including destruction, are called under the owning LVGL lock.
// The widget owns exactly one timer, with no per-frame object allocations.
class RobotFleetWidget {
public:
    explicit RobotFleetWidget(lv_obj_t* parent, int width, int height);
    ~RobotFleetWidget();
    RobotFleetWidget(const RobotFleetWidget&) = delete;
    RobotFleetWidget& operator=(const RobotFleetWidget&) = delete;

    void SetActivity(Activity activity);
    void SetVisible(bool visible);
    void SetAccentColor(lv_color_t color);
    lv_obj_t* root() const { return root_; }
    bool animation_running() const { return timer_ != nullptr && !lv_timer_get_paused(timer_); }

private:
    static void OnTimer(lv_timer_t* timer);
    void RenderFrame(uint32_t elapsed_ms);
    void UpdateTimer();

    Layout layout_;
    Activity activity_ = Activity::Idle;
    bool visible_ = true;
    uint32_t started_at_ = 0;
    lv_obj_t* root_ = nullptr;
    lv_obj_t* halo_ = nullptr;
    lv_obj_t* ring_ = nullptr;
    lv_obj_t* arc_ = nullptr;
    lv_obj_t* logo_ = nullptr;
    std::array<lv_obj_t*, 7> wave_bars_{};
    lv_timer_t* timer_ = nullptr;
};

}  // namespace robotfleet

#endif  // ROBOTFLEET_WIDGET_H
