#include "newpipe/google_device.hpp"

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace {
using namespace newpipe;
struct MockHttp : HttpClient {
    struct Call { std::string url, body; std::vector<HttpHeader> headers; };
    std::vector<Call> calls;
    std::vector<HttpResponse> responses;

    std::optional<HttpResponse> request(const std::string& url, const std::vector<HttpHeader>& headers,
                                       const std::string* body, bool follow) override {
        assert(!follow && url.rfind("https://", 0) == 0 && !responses.empty());
        calls.push_back({url, body ? *body : "", headers});
        auto result = responses.front();
        responses.erase(responses.begin());
        return result;
    }
};
}  // namespace

int main() {
    using namespace newpipe;
    char temp[] = "/tmp/kaitube-device-XXXXXX";
    if (!mkdtemp(temp)) return 1;
    auto original = std::filesystem::current_path();
    std::filesystem::current_path(temp);
    std::string error;
    assert(!load_google_client(error));
    std::ofstream(google_client_path()) << R"({"installed":{"client_id":"test-client","client_secret":"secret&test"}})";
    auto client = load_google_client(error);
    assert(client && client->id == "test-client");
    MockHttp http;
    http.responses.push_back({200, R"({"user_code":"FOO-BAR","device_code":"private+code","verification_url":"https://www.google.com/device","expires_in":1800,"interval":10})"});
    auto code = request_device_code(http, *client, error);
    assert(code && code->interval_seconds == 10 && code->user_code == "FOO-BAR");
    assert(http.calls.back().body == "client_id=test-client&scope=https%3A%2F%2Fwww.googleapis.com%2Fauth%2Fyoutube.readonly");
    http.responses = {{428, R"({"error":"authorization_pending"})"}, {403, R"({"error":"slow_down"})"},
                      {503, ""}, {403, R"({"error":"access_denied"})"}, {200, "{}"}};
    for (auto state : {PollState::pending, PollState::slow_down, PollState::retry, PollState::failed, PollState::failed})
        assert(poll_device_code(http, *client, *code).state == state);
    assert(http.calls.back().body.find("device_code=private%2Bcode") != std::string::npos);
    assert(http.calls.back().body.find("client_secret=secret%26test") != std::string::npos);
    auto expired = *code;
    expired.deadline = std::chrono::steady_clock::now();
    assert(poll_device_code(http, *client, expired).state == PollState::failed);

    http.responses.push_back({200, R"({"token_type":"Bearer","scope":"unrelated","access_token":"denied","refresh_token":"test","expires_in":600})"});
    assert(poll_device_code(http, *client, *code).state == PollState::failed);
    http.responses.push_back({200, R"({"token_type":"Bearer","access_token":"access-one","refresh_token":"refresh-one","expires_in":3600})"});
    auto granted = poll_device_code(http, *client, *code);
    auto& store = AuthStore::instance();
    assert(store.update_from_cookie_header("SAPISID=cookie", "test", &error));
    assert(granted.state == PollState::granted && store.save_google_tokens(granted.tokens, error));
    assert(store.has_session() && store.session().device_login() && !store.session().has_cookies());
    assert(store.build_youtube_headers("https://www.youtube.com", "", &error).empty());

    assert(store.reload() && store.load());
    assert(store.session().google.access_token.empty() && store.session().google.refresh_token == "refresh-one");
    http.responses = {
        {200, R"({"token_type":"Bearer","access_token":"access-two","expires_in":3600})"},
        {401, "{}"},
        {200, R"({"token_type":"Bearer","access_token":"access-three","refresh_token":"refresh-two","expires_in":3600})"},
        {200, R"({"nextPageToken":"next/+","items":[{"snippet":{"title":"First","resourceId":{"channelId":"channelA"}}}]})"},
        {200, R"({"items":[{"snippet":{"title":"First","resourceId":{"channelId":"channelA"}}},{"snippet":{"title":"Second","resourceId":{"channelId":"channelB"}}}]})"}
    };
    auto feed = google_subscriptions(http, store, error);
    assert(feed && feed->items.size() == 2 && feed->items.back().title == "Second");
    assert(http.calls.back().url.find("pageToken=next%2F%2B") != std::string::npos);
    assert(http.calls.back().headers.at(0).value == "Bearer access-three");
    assert(store.session().google.refresh_token == "refresh-two");

    assert(store.reload() && store.load());
    http.responses.push_back({503, ""});
    assert(!google_subscriptions(http, store, error) && store.session().device_login());
    http.responses.push_back({400, R"({"error":"invalid_grant"})"});
    assert(!google_subscriptions(http, store, error) && !error.empty());
    const auto old = store.session().google;
    assert(store.clear() && !store.save_google_tokens(granted.tokens, error, &old));
    assert(store.reload() && store.load() && !store.has_session());
    std::filesystem::current_path(original);
    std::filesystem::remove_all(temp);
    std::cout << "Device grant, polling, session replacement, refresh, subscriptions and sign-out passed\n";
}
