#include "notification_channel.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <memory>
#include <mutex>

#include <esp_log.h>
#include <cJSON.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "board.h"
#include "settings.h"
#include "system_info.h"
#include "web_socket.h"

namespace {
constexpr const char* kTag = "NoticeLink";
using Clock = std::chrono::steady_clock;

bool ValidId(const std::string& value) {
    if (value.size() < 4 || value.size() > 64 || value.compare(0, 3, "wn_") != 0) {
        return false;
    }
    for (unsigned char c : value) {
        if (!std::isalnum(c) && c != '_' && c != '-') {
            return false;
        }
    }
    return true;
}
}  // namespace

struct NotificationChannel::State {
    std::mutex mutex;
    Receiver receiver;
    std::atomic<bool> stopped{false};
    std::atomic<bool> connected{false};
    std::atomic<DeviceState> device_state{kDeviceStateStarting};
    std::atomic<bool> dirty{true};
    std::string request_id;
    std::string reply_id;
    std::string reply_status;
    std::string audio_prefix;
    Clock::time_point pending_since;
    Clock::time_point last_pong;

    void Incoming(const char* data, size_t len, bool binary) {
        if (binary || len > 1024 || stopped.load()) {
            return;
        }
        std::unique_ptr<cJSON, decltype(&cJSON_Delete)> root(cJSON_ParseWithLength(data, len),
                                                             cJSON_Delete);
        if (!root) {
            return;
        }
        auto type = cJSON_GetObjectItemCaseSensitive(root.get(), "type");
        auto id = cJSON_GetObjectItemCaseSensitive(root.get(), "request_id");
        auto phase = cJSON_GetObjectItemCaseSensitive(root.get(), "phase");
        if (!cJSON_IsString(type) || std::string(type->valuestring) != "web_notice" ||
            !cJSON_IsString(id) || !ValidId(id->valuestring) || !cJSON_IsString(phase)) {
            return;
        }
        std::string outcome;
        std::string text;
        std::string action(phase->valuestring);
        if (action == "prepare") {
            auto kind = cJSON_GetObjectItemCaseSensitive(root.get(), "outcome");
            auto message = cJSON_GetObjectItemCaseSensitive(root.get(), "text");
            if (!cJSON_IsString(kind) || !cJSON_IsString(message)) {
                return;
            }
            outcome = kind->valuestring;
            text = message->valuestring;
            if ((outcome != "success" && outcome != "partial" && outcome != "failed") ||
                text.size() > 256) {
                return;
            }
        } else if (action != "play" && action != "cancel") {
            return;
        }
        Receiver callback;
        std::string url;
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (stopped.load()) {
                return;
            }
            if (action == "prepare") {
                if (!request_id.empty() || device_state.load() != kDeviceStateIdle) {
                    // A single bounded reply slot; the server sends one request at a time.
                    reply_id = id->valuestring;
                    reply_status = "busy";
                    return;
                }
                request_id = id->valuestring;
                pending_since = Clock::now();
                url = audio_prefix + outcome + ".ogg";
            } else if (request_id != id->valuestring) {
                return;
            }
            callback = receiver;
        }
        if (callback) {
            callback(id->valuestring, action, url, text);
        }
    }

    void Run() {
        Settings settings("websocket", false);
        const std::string url = settings.GetString("url");
        std::string token = settings.GetString("token");
        const int version = settings.GetInt("version");
        if (url.compare(0, 6, "wss://") != 0 || url.size() < 16 ||
            url.compare(url.size() - 10, 10, "/voice/ws/") != 0 || token.empty()) {
            ESP_LOGW(kTag, "Standby configuration unavailable");
            return;
        }
        audio_prefix = "https://" + url.substr(6) + "notices/";
        if (token.find(' ') == std::string::npos) {
            token = "Bearer " + token;
        }
        int backoff = 2;
        while (!stopped.load()) {
            // Reconnection cannot compete with an in-progress spoken request.
            if (device_state.load() != kDeviceStateIdle) {
                vTaskDelay(pdMS_TO_TICKS(250));
                continue;
            }
            auto socket = Board::GetInstance().GetNetwork()->CreateWebSocket(2);
            if (!socket) {
                break;
            }
            socket->SetHeader("Authorization", token.c_str());
            socket->SetHeader("Device-Id", SystemInfo::GetMacAddress().c_str());
            socket->SetHeader("Client-Id", Board::GetInstance().GetUuid().c_str());
            socket->SetHeader("Protocol-Version", std::to_string(version ? version : 1).c_str());
            socket->OnData(
                [this](const char* data, size_t len, bool binary) { Incoming(data, len, binary); });
            socket->OnDisconnected([this]() { connected.store(false); });
            socket->OnPong([this](const char*, size_t) {
                std::lock_guard<std::mutex> lock(mutex);
                last_pong = Clock::now();
            });
            auto opened = socket->Connect((url + "notifications/").c_str());
            if (opened) {
                ESP_LOGI(kTag, "Standby notification link connected");
                connected.store(true);
                dirty.store(true);
                auto next_report = Clock::now();
                {
                    std::lock_guard<std::mutex> lock(mutex);
                    last_pong = Clock::now();
                }
                backoff = 2;
                while (!stopped.load() && connected.load() && socket->IsConnected()) {
                    auto now = Clock::now();
                    std::string id;
                    std::string status;
                    bool expired;
                    {
                        std::lock_guard<std::mutex> lock(mutex);
                        id.swap(reply_id);
                        status.swap(reply_status);
                        expired = now - last_pong > std::chrono::seconds(30);
                        if (!request_id.empty() && now - pending_since > std::chrono::seconds(30)) {
                            request_id.clear();
                        }
                    }
                    if (expired) {
                        break;
                    }
                    if (!id.empty()) {
                        std::string ack = "{\"type\":\"ack\",\"request_id\":\"" + id +
                                          "\",\"status\":\"" + status + "\"}";
                        if (!socket->Send(ack)) {
                            break;
                        }
                    }
                    if (dirty.exchange(false) || now >= next_report) {
                        auto value = device_state.load();
                        const char* state = value == kDeviceStateIdle        ? "idle"
                                            : value == kDeviceStateNotifying ? "notifying"
                                                                             : "busy";
                        if (!socket->Send(std::string("{\"type\":\"state\",\"state\":\"") + state +
                                          "\"}")) {
                            break;
                        }
                        socket->Ping();
                        next_report = now + std::chrono::seconds(10);
                    }
                    vTaskDelay(pdMS_TO_TICKS(100));
                }
            }
            connected.store(false);
            socket.reset();
            {
                std::lock_guard<std::mutex> lock(mutex);
                request_id.clear();
                reply_id.clear();
                reply_status.clear();
            }
            for (int tick = 0; tick < backoff * 4 && !stopped.load(); ++tick) {
                vTaskDelay(pdMS_TO_TICKS(250));
            }
            backoff = std::min(30, backoff * 2);
        }
    }
};

NotificationChannel::NotificationChannel(Receiver receiver) : state_(std::make_shared<State>()) {
    state_->receiver = std::move(receiver);
}

NotificationChannel::~NotificationChannel() { Stop(); }

bool NotificationChannel::Start() {
    Settings settings("websocket", false);
    if (settings.GetInt("notice") != 1) {
        return false;
    }
    auto owner = new std::shared_ptr<State>(state_);
    BaseType_t created = xTaskCreate(
        [](void* arg) {
            std::unique_ptr<std::shared_ptr<State>> state(
                static_cast<std::shared_ptr<State>*>(arg));
            (*state)->Run();
            state.reset();
            vTaskDelete(nullptr);
        },
        "notice_link", 6144, owner, 2, nullptr);
    if (created != pdPASS) {
        delete owner;
        return false;
    }
    return true;
}

void NotificationChannel::Stop() {
    state_->stopped.store(true);
    std::lock_guard<std::mutex> lock(state_->mutex);
    state_->receiver = nullptr;
}

void NotificationChannel::SetState(DeviceState state) {
    state_->device_state.store(state);
    state_->dirty.store(true);
}

bool NotificationChannel::IsCurrent(const std::string& request_id) const {
    std::lock_guard<std::mutex> lock(state_->mutex);
    return state_->connected.load() && !state_->stopped.load() && state_->request_id == request_id;
}

void NotificationChannel::Reply(const std::string& request_id, const std::string& status) {
    std::lock_guard<std::mutex> lock(state_->mutex);
    if (state_->request_id != request_id || !state_->connected.load()) {
        return;
    }
    state_->reply_id = request_id;
    state_->reply_status = status;
    if (status == "completed" || status == "failed" || status == "cancelled" || status == "busy") {
        state_->request_id.clear();
    }
}
