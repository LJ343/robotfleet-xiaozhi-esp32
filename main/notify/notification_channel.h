#ifndef NOTIFICATION_CHANNEL_H_
#define NOTIFICATION_CHANNEL_H_

#include <functional>
#include <memory>
#include <string>

#include "device_state.h"

// A separate, optional, authenticated standby link. It never captures audio.
// Network work stays in its own task; main-loop methods only update bounded state.
class NotificationChannel {
public:
    using Receiver = std::function<void(std::string, std::string, std::string, std::string)>;
    explicit NotificationChannel(Receiver receiver);
    ~NotificationChannel();
    bool Start();
    void Stop();
    void SetState(DeviceState state);
    bool IsCurrent(const std::string& request_id) const;
    void Reply(const std::string& request_id, const std::string& status);

private:
    struct State;
    std::shared_ptr<State> state_;
};

#endif  // NOTIFICATION_CHANNEL_H_
