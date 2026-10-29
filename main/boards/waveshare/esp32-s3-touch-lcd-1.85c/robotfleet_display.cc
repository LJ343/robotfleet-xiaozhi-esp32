#include "robotfleet_display.h"

#include <esp_err.h>
#include <algorithm>

namespace {

robotfleet::Activity ToWidgetActivity(DisplayActivity activity) {
    switch (activity) {
        case DisplayActivity::Idle:
            return robotfleet::Activity::Idle;
        case DisplayActivity::Connecting:
            return robotfleet::Activity::Connecting;
        case DisplayActivity::Listening:
            return robotfleet::Activity::Listening;
        case DisplayActivity::Thinking:
            return robotfleet::Activity::Thinking;
        case DisplayActivity::Speaking:
            return robotfleet::Activity::Speaking;
        case DisplayActivity::Offline:
            return robotfleet::Activity::Offline;
        case DisplayActivity::Other:
            return robotfleet::Activity::Other;
    }
    return robotfleet::Activity::Other;
}

}  // namespace

RobotFleetDisplay::~RobotFleetDisplay() {
    DisplayLockGuard lock(this);
    brand_widget_.reset();
}

void RobotFleetDisplay::SetupUI() {
    if (setup_ui_called_) {
        return;
    }
    LcdDisplay::SetupUI();
#if !CONFIG_USE_WECHAT_MESSAGE_STYLE
    DisplayLockGuard lock(this);
    HideLegacyEmotionLocked();

    // The original background is the screen's first child. Placing the brand
    // here keeps status, subtitles, previews and battery alerts in front of it.
    brand_widget_ = std::make_unique<robotfleet::RobotFleetWidget>(container_, width_, height_);
    brand_widget_->SetActivity(activity_);
#endif
}

void RobotFleetDisplay::SetActivity(DisplayActivity activity) {
    DisplayLockGuard lock(this);
    activity_ = ToWidgetActivity(activity);
    if (brand_widget_) {
        brand_widget_->SetActivity(activity_);
    }
}

void RobotFleetDisplay::HideLegacyEmotionLocked() {
    if (gif_controller_) {
        gif_controller_->Stop();
        gif_controller_.reset();
    }
    if (emoji_box_ != nullptr) {
        lv_obj_add_flag(emoji_box_, LV_OBJ_FLAG_HIDDEN);
    }
    if (emoji_label_ != nullptr) {
        lv_obj_add_flag(emoji_label_, LV_OBJ_FLAG_HIDDEN);
    }
    if (emoji_image_ != nullptr) {
        lv_obj_add_flag(emoji_image_, LV_OBJ_FLAG_HIDDEN);
    }
}

void RobotFleetDisplay::SetEmotion(const char* emotion) {
#if CONFIG_USE_WECHAT_MESSAGE_STYLE
    LcdDisplay::SetEmotion(emotion);
#else
    // Server emotion names do not select local activity or replace the logo.
    (void)emotion;
    DisplayLockGuard lock(this);
    HideLegacyEmotionLocked();
#endif
}

void RobotFleetDisplay::SetPreviewImage(std::unique_ptr<LvglImage> image) {
#if CONFIG_USE_WECHAT_MESSAGE_STYLE
    // Retain the optional chat-bubble preview behavior if this board is built
    // with that layout. Its foreground content remains owned by LcdDisplay.
    LcdDisplay::SetPreviewImage(std::move(image));
#else
    DisplayLockGuard lock(this);
    if (preview_image_ == nullptr) {
        return;
    }
    esp_timer_stop(preview_timer_);
    if (image == nullptr) {
        lv_obj_add_flag(preview_image_, LV_OBJ_FLAG_HIDDEN);
        preview_image_cached_.reset();
        if (brand_widget_) {
            brand_widget_->SetVisible(true);
        }
        HideLegacyEmotionLocked();
        return;
    }

    preview_image_cached_ = std::move(image);
    const auto descriptor = preview_image_cached_->image_dsc();
    lv_image_set_src(preview_image_, descriptor);
    if (descriptor->header.w > 0 && descriptor->header.h > 0) {
        // Preserve the existing half-screen preview, including its timeout.
        // Bound both dimensions so a portrait image cannot cover the bars.
        const int largest_dimension = std::max(static_cast<int>(descriptor->header.w),
                                               static_cast<int>(descriptor->header.h));
        lv_image_set_scale(preview_image_, 128 * std::min(width_, height_) / largest_dimension);
    }
    HideLegacyEmotionLocked();
    if (brand_widget_) {
        brand_widget_->SetVisible(false);
    }
    lv_obj_remove_flag(preview_image_, LV_OBJ_FLAG_HIDDEN);
    ESP_ERROR_CHECK(esp_timer_start_once(preview_timer_, PREVIEW_IMAGE_DURATION_MS * 1000));
#endif
}

void RobotFleetDisplay::ClearChatMessages() {
    LcdDisplay::ClearChatMessages();
#if !CONFIG_USE_WECHAT_MESSAGE_STYLE
    DisplayLockGuard lock(this);
    HideLegacyEmotionLocked();
#endif
}

void RobotFleetDisplay::SetTheme(Theme* theme) {
    if (!setup_ui_called_) {
        Display::SetTheme(theme);
        return;
    }
    LcdDisplay::SetTheme(theme);
#if !CONFIG_USE_WECHAT_MESSAGE_STYLE
    DisplayLockGuard lock(this);
    HideLegacyEmotionLocked();
    if (brand_widget_) {
        // The supplied logo pixels remain untouched in light and dark themes.
        brand_widget_->SetAccentColor(lv_color_hex(0x229483));
    }
#endif
}
