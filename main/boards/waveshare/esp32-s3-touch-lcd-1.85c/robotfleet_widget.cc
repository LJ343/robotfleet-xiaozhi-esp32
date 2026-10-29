#include "robotfleet_widget.h"

#include "robotfleet_logo.h"

namespace robotfleet {
namespace {

void MakePassive(lv_obj_t* object) {
    lv_obj_remove_flag(object, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(object, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(object, LV_SCROLLBAR_MODE_OFF);
}

void Show(lv_obj_t* object, bool visible) {
    if (visible) {
        lv_obj_remove_flag(object, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(object, LV_OBJ_FLAG_HIDDEN);
    }
}

lv_obj_t* MakeCircle(lv_obj_t* parent, int diameter, int border_width) {
    auto circle = lv_obj_create(parent);
    lv_obj_remove_style_all(circle);
    MakePassive(circle);
    lv_obj_set_size(circle, diameter, diameter);
    lv_obj_set_style_radius(circle, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(circle, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(circle, border_width, 0);
    return circle;
}

}  // namespace

RobotFleetWidget::RobotFleetWidget(lv_obj_t* parent, int width, int height)
    : layout_(MakeLayout(width, height)), started_at_(lv_tick_get()) {
    root_ = lv_obj_create(parent);
    lv_obj_remove_style_all(root_);
    lv_obj_set_size(root_, width, height);
    lv_obj_set_pos(root_, 0, 0);
    lv_obj_add_flag(root_, LV_OBJ_FLAG_IGNORE_LAYOUT);
    MakePassive(root_);

    // Draw the subtle halo first, behind the crisp main circle and static logo.
    halo_ = MakeCircle(root_, kRingMinDiameter + kHaloExpansion, 5);
    ring_ = MakeCircle(root_, kRingMinDiameter, 2);

    arc_ = lv_arc_create(root_);
    lv_obj_remove_style_all(arc_);
    MakePassive(arc_);
    lv_obj_set_size(arc_, kArcDiameter, kArcDiameter);
    lv_obj_set_pos(arc_, layout_.center_x - kArcDiameter / 2, layout_.center_y - kArcDiameter / 2);
    lv_obj_set_style_arc_opa(arc_, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc_, 3, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(arc_, true, LV_PART_INDICATOR);
    lv_arc_set_bg_angles(arc_, 0, 360);
    lv_arc_set_angles(arc_, 0, 58);

    logo_ = lv_image_create(root_);
    MakePassive(logo_);
    lv_image_set_src(logo_, &kRobotFleetLogo);
    lv_obj_set_pos(logo_, layout_.center_x - layout_.logo_size / 2,
                   layout_.center_y - layout_.logo_size / 2);

    for (size_t i = 0; i < wave_bars_.size(); ++i) {
        auto bar = lv_obj_create(root_);
        lv_obj_remove_style_all(bar);
        MakePassive(bar);
        lv_obj_set_style_radius(bar, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(bar, LV_OPA_80, 0);
        wave_bars_[i] = bar;
    }
    SetAccentColor(lv_color_hex(0x229483));
    RenderFrame(0);
    timer_ = lv_timer_create(OnTimer, 50, this);
    lv_timer_pause(timer_);
}

RobotFleetWidget::~RobotFleetWidget() {
    // Never leave a timer holding `this` while deleting its object tree.
    if (timer_ != nullptr) {
        lv_timer_delete(timer_);
        timer_ = nullptr;
    }
    if (root_ != nullptr) {
        lv_obj_delete(root_);
        root_ = nullptr;
    }
}

void RobotFleetWidget::SetActivity(Activity activity) {
    if (activity_ == activity) {
        return;
    }
    activity_ = activity;
    started_at_ = lv_tick_get();
    RenderFrame(0);
    UpdateTimer();
}

void RobotFleetWidget::SetVisible(bool visible) {
    if (visible_ == visible) {
        return;
    }
    visible_ = visible;
    Show(root_, visible);
    if (visible) {
        RenderFrame(lv_tick_elaps(started_at_));
    }
    UpdateTimer();
}

void RobotFleetWidget::SetAccentColor(lv_color_t color) {
    lv_obj_set_style_border_color(halo_, color, 0);
    lv_obj_set_style_border_color(ring_, color, 0);
    lv_obj_set_style_arc_color(arc_, color, LV_PART_INDICATOR);
    for (auto bar : wave_bars_) {
        lv_obj_set_style_bg_color(bar, color, 0);
    }
}

void RobotFleetWidget::OnTimer(lv_timer_t* timer) {
    auto widget = static_cast<RobotFleetWidget*>(lv_timer_get_user_data(timer));
    widget->RenderFrame(lv_tick_elaps(widget->started_at_));
}

void RobotFleetWidget::UpdateTimer() {
    if (visible_ && IsAnimated(activity_)) {
        lv_timer_reset(timer_);
        lv_timer_resume(timer_);
    } else {
        lv_timer_pause(timer_);
    }
}

void RobotFleetWidget::RenderFrame(uint32_t elapsed_ms) {
    const auto frame = FrameAt(activity_, elapsed_ms);
    const bool listening = activity_ == Activity::Listening;
    Show(halo_, listening);
    Show(ring_, listening);
    if (listening) {
        const int diameter = frame.ring_diameter;
        lv_obj_set_size(ring_, diameter, diameter);
        lv_obj_set_pos(ring_, layout_.center_x - diameter / 2, layout_.center_y - diameter / 2);
        lv_obj_set_style_border_opa(ring_, frame.ring_opacity, 0);
        const int halo_diameter = diameter + kHaloExpansion;
        lv_obj_set_size(halo_, halo_diameter, halo_diameter);
        lv_obj_set_pos(halo_, layout_.center_x - halo_diameter / 2,
                       layout_.center_y - halo_diameter / 2);
        lv_obj_set_style_border_opa(halo_, frame.halo_opacity, 0);
    }

    const bool waiting = activity_ == Activity::Connecting || activity_ == Activity::Thinking;
    Show(arc_, waiting);
    if (waiting) {
        lv_arc_set_rotation(arc_, frame.arc_rotation);
    }

    const bool speaking = activity_ == Activity::Speaking;
    for (size_t i = 0; i < wave_bars_.size(); ++i) {
        auto bar = wave_bars_[i];
        Show(bar, speaking);
        if (speaking) {
            const int height = frame.wave_heights[i];
            lv_obj_set_size(bar, 4, height);
            lv_obj_set_pos(bar, layout_.center_x - 26 + static_cast<int>(i) * 8,
                           layout_.wave_center_y - height / 2);
        }
    }
}

}  // namespace robotfleet
