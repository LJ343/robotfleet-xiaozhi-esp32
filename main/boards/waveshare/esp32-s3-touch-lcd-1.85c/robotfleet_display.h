#ifndef ROBOTFLEET_DISPLAY_H
#define ROBOTFLEET_DISPLAY_H

#include "display/lcd_display.h"
#include "robotfleet_widget.h"

// This UI belongs only to the Waveshare 360x360 board. Other boards continue to
// use their existing displays and emotion rendering.
class RobotFleetDisplay : public SpiLcdDisplay {
public:
    using SpiLcdDisplay::SpiLcdDisplay;
    ~RobotFleetDisplay() override;

    void SetupUI() override;
    void SetActivity(DisplayActivity activity) override;
    void SetEmotion(const char* emotion) override;
    void SetPreviewImage(std::unique_ptr<LvglImage> image) override;
    void ClearChatMessages() override;
    void SetTheme(Theme* theme) override;

private:
    void HideLegacyEmotionLocked();

    robotfleet::Activity activity_ = robotfleet::Activity::Idle;
    std::unique_ptr<robotfleet::RobotFleetWidget> brand_widget_;
};

#endif  // ROBOTFLEET_DISPLAY_H
