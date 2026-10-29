import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]


class DisplayActivityTest(unittest.TestCase):
    def test_real_presentation_reducer_transitions(self):
        driver = r'''
        #include "display_activity.h"
        #include <cassert>
        #include <iostream>

        int main() {
            DisplayActivityState ui;
            using A = DisplayActivity;
            assert(ui.activity() == A::Other);
            ui.OnState(kDeviceStateStarting);
            assert(ui.activity() == A::Other);
            ui.OnState(kDeviceStateWifiConfiguring);
            assert(ui.activity() == A::Other);
            ui.OnNetwork(true);
            ui.OnState(kDeviceStateIdle);
            assert(ui.activity() == A::Idle);
            ui.OnState(kDeviceStateConnecting);
            assert(ui.activity() == A::Connecting);
            ui.OnState(kDeviceStateListening);
            assert(ui.activity() == A::Listening);

            // Manual-stop input uses Idle while the accepted request waits.
            ui.OnRecognizedText(true);
            ui.OnState(kDeviceStateIdle);
            assert(ui.activity() == A::Thinking);
            ui.OnTtsStart();
            ui.OnState(kDeviceStateSpeaking);
            ui.OnAssistantSentence("请稍候。");
            ui.OnTtsStop();
            ui.OnState(kDeviceStateIdle);
            assert(ui.activity() == A::Thinking);
            ui.OnTtsStart();
            ui.OnState(kDeviceStateSpeaking);
            ui.OnAssistantSentence("手动模式的最终回答。");
            ui.OnTtsStop();
            ui.OnState(kDeviceStateIdle);
            assert(ui.activity() == A::Idle);
            ui.OnState(kDeviceStateListening);

            // Manual and voice wake both reach listening without a UI greeting.
            ui.OnRecognizedText(false);
            assert(ui.activity() == A::Listening);
            ui.OnRecognizedText(true);
            assert(ui.activity() == A::Thinking);
            ui.OnTtsStart();
            ui.OnState(kDeviceStateSpeaking);
            ui.OnAssistantSentence("请稍候。");
            assert(ui.activity() == A::Speaking);
            ui.OnTtsStop();
            ui.OnState(kDeviceStateListening);
            assert(ui.activity() == A::Thinking);

            // Normal answer is distinct from the fixed waiting cue.
            ui.OnTtsStart();
            ui.OnState(kDeviceStateSpeaking);
            ui.OnAssistantSentence("这是回答正文。");
            assert(ui.activity() == A::Speaking);
            ui.OnTtsStop();
            ui.OnState(kDeviceStateListening);
            assert(ui.activity() == A::Listening);

            // Current gateway leaves TTS open across three notices and then
            // sends final text without an intermediate STOP/re-listen cycle.
            ui.OnRecognizedText(true);
            ui.OnTtsStart();
            ui.OnState(kDeviceStateSpeaking);
            for (int notice = 0; notice < 3; ++notice) {
                ui.OnAssistantSentence("正在处理，请稍候。");
                assert(ui.activity(false) == A::Speaking);
                assert(ui.activity(true) == A::Thinking);
                // A temporary stream gap may refill within the same sentence.
                assert(ui.activity(false) == A::Speaking);
                assert(ui.activity(true) == A::Thinking);
            }
            ui.OnAssistantSentence("这是最终回答。");
            assert(ui.activity(true) == A::Speaking);
            ui.OnTtsStop();
            ui.OnState(kDeviceStateListening);
            assert(ui.activity(true) == A::Listening);

            // Durable task acceptance/status ends normally; it must not leave
            // a spinner waiting for a result from an earlier conversation.
            ui.OnRecognizedText(true);
            ui.OnTtsStart();
            ui.OnState(kDeviceStateSpeaking);
            ui.OnAssistantSentence("任务已提交，完成后通知你。");
            ui.OnTtsStop();
            ui.OnState(kDeviceStateListening);
            assert(ui.activity(true) == A::Listening);

            // New speech/physical interruption must clear the old waiting UI.
            ui.OnRecognizedText(true);
            ui.OnUserSpeech();
            assert(ui.activity() == A::Listening);
            ui.OnRecognizedText(true);
            ui.OnInterrupt();
            assert(ui.activity() == A::Listening);

            // An unsolicited phrase is never treated as a pending request.
            ui.OnTtsStart();
            ui.OnState(kDeviceStateSpeaking);
            ui.OnAssistantSentence("请稍候。");
            ui.OnTtsStop();
            ui.OnState(kDeviceStateListening);
            assert(ui.activity() == A::Listening);

            // Disconnect/error wins over listening, playback or spinning.
            ui.OnRecognizedText(true);
            ui.OnNetwork(false);
            assert(ui.activity() == A::Offline);
            ui.OnState(kDeviceStateSpeaking);
            assert(ui.activity() == A::Offline);
            ui.OnState(kDeviceStateIdle);
            assert(ui.activity() == A::Offline);
            ui.OnNetwork(true);
            assert(ui.activity() == A::Idle);
            ui.OnState(kDeviceStateListening);
            ui.OnRecognizedText(true);
            ui.OnError();
            assert(ui.activity() == A::Offline);
            ui.OnState(kDeviceStateIdle);
            assert(ui.activity() == A::Offline);
            ui.OnState(kDeviceStateConnecting);
            assert(ui.activity() == A::Connecting);
            ui.OnState(kDeviceStateListening);
            assert(ui.activity() == A::Listening);

            // Channel closure clears a pending spinner even with Wi-Fi intact.
            ui.OnRecognizedText(true);
            ui.OnChannelClosed();
            ui.OnState(kDeviceStateIdle);
            assert(ui.activity() == A::Idle);
            ui.OnState(kDeviceStateNotifying);
            assert(ui.activity() == A::Speaking);
            ui.OnState(kDeviceStateUpgrading);
            assert(ui.activity() == A::Other);

            // Only exact existing cues are compatible; arbitrary response
            // containing the same words is still treated as an answer.
            ui.OnState(kDeviceStateListening);
            ui.OnRecognizedText(true);
            ui.OnTtsStart();
            ui.OnState(kDeviceStateSpeaking);
            ui.OnAssistantSentence("请稍候，我来解释操作方法。");
            ui.OnTtsStop();
            ui.OnState(kDeviceStateListening);
            assert(ui.activity() == A::Listening);
            std::cout << "display activity transition scenarios passed\n";
        }
        '''
        with tempfile.TemporaryDirectory(prefix="display-activity-") as temp:
            source = Path(temp) / "driver.cc"
            output = Path(temp) / "driver"
            source.write_text(driver)
            compiler = shlex.split(os.environ.get("CXX", "g++"))
            subprocess.run(
                compiler + ["-std=c++17", "-Wall", "-Wextra", "-Werror",
                            "-I", str(ROOT / "main"), "-I", str(ROOT / "main/display"),
                            str(source), "-o", str(output)],
                check=True, capture_output=True, text=True,
            )
            subprocess.run([str(output)], check=True, capture_output=True, text=True)
