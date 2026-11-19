#pragma once

#include "newpipe/auth_store.hpp"
#include "newpipe/models.hpp"

namespace newpipe {

struct GoogleClient {
    std::string id;
    std::string secret;
};

struct DeviceCode {
    std::string device_code;
    std::string user_code;
    std::string verification_url;
    std::chrono::steady_clock::time_point deadline;
    int interval_seconds = 5;
};

enum class PollState { pending, slow_down, retry, granted, failed };

struct PollResult {
    PollState state;
    GoogleTokens tokens;
    std::string error;
};

std::string google_client_path();
std::optional<GoogleClient> load_google_client(std::string& error);
std::optional<DeviceCode> request_device_code(HttpClient& http, const GoogleClient& client,
                                             std::string& error);
PollResult poll_device_code(HttpClient& http, const GoogleClient& client, const DeviceCode& code);
std::optional<HomeFeed> google_subscriptions(HttpClient& http, AuthStore& store, std::string& error);

}  // namespace newpipe
