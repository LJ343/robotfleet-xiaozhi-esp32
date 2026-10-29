#pragma once

#include "device_state.h"

#include <string_view>

// Presentation only. Never changes recording, playback, transport or Core tasks.
enum class DisplayActivity { Idle, Connecting, Listening, Thinking, Speaking, Offline, Other };

class DisplayActivityState {
public:
    void OnState(DeviceState state) {
        state_ = state;
        if (state == kDeviceStateConnecting || state == kDeviceStateWifiConfiguring ||
            state == kDeviceStateActivating || state == kDeviceStateUpgrading ||
            state == kDeviceStateFatalError) {
            ClearPending();
        }
        if (state == kDeviceStateConnecting) {
            failed_ = false;
        }
    }

    void OnNetwork(bool connected) {
        network_available_ = connected;
        failed_ = false;
        ClearPending();
    }

    void OnRecognizedText(bool nonempty) {
        if (nonempty) {
            pending_ = true;
            notice_ = false;
        }
    }

    void OnTtsStart() { notice_ = false; }

    void OnAssistantSentence(std::string_view text) {
        // Existing gateways do not label notice audio. Recognize only their
        // exact fixed cue, and only after STT. No business intent is inferred.
        notice_ = pending_ && (text == "请稍候。" || text == "请稍候" || text == "请稍候." ||
                               text == "正在处理，请稍候。");
        if (!notice_) {
            pending_ = false;
        }
    }

    void OnTtsStop() {
        if (!notice_) {
            pending_ = false;
        }
        notice_ = false;
    }

    void OnUserSpeech() { ClearPending(); }
    void OnInterrupt() { ClearPending(); }
    void OnChannelClosed() { ClearPending(); }
    void OnError() {
        failed_ = true;
        ClearPending();
    }

    DisplayActivity activity(bool playback_idle = false) const {
        switch (state_) {
            case kDeviceStateIdle:
            case kDeviceStateConnecting:
            case kDeviceStateListening:
            case kDeviceStateSpeaking:
            case kDeviceStateNotifying:
                if (!network_available_ || failed_) {
                    return DisplayActivity::Offline;
                }
                break;
            default:
                return DisplayActivity::Other;
        }
        switch (state_) {
            case kDeviceStateIdle:
                return pending_ ? DisplayActivity::Thinking : DisplayActivity::Idle;
            case kDeviceStateConnecting:
                return DisplayActivity::Connecting;
            case kDeviceStateListening:
                return pending_ ? DisplayActivity::Thinking : DisplayActivity::Listening;
            case kDeviceStateSpeaking:
                // New gateways keep TTS open across waiting notices. A local
                // queue gap shows waiting without changing audio/listening.
                return pending_ && notice_ && playback_idle ? DisplayActivity::Thinking
                                                            : DisplayActivity::Speaking;
            case kDeviceStateNotifying:
                return DisplayActivity::Speaking;
            default:
                return DisplayActivity::Other;
        }
    }

private:
    void ClearPending() {
        pending_ = false;
        notice_ = false;
    }

    DeviceState state_ = kDeviceStateUnknown;
    bool network_available_ = false;
    bool failed_ = false;
    bool pending_ = false;
    bool notice_ = false;
};
