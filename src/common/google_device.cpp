#include "newpipe/google_device.hpp"
#include "newpipe/ui_text.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <limits>
#include <sstream>
#include <unordered_set>

namespace newpipe {
namespace {
using nlohmann::json;
using Clock = std::chrono::steady_clock;
constexpr char scope[] = "https://www.googleapis.com/auth/youtube.readonly";
constexpr char token_url[] = "https://oauth2.googleapis.com/token";
const std::vector<HttpHeader> form_headers{{"Content-Type", "application/x-www-form-urlencoded"}};

std::string field(const json& object, const char* name) {
    const auto it = object.find(name);
    return it != object.end() && it->is_string() ? it->get<std::string>() : "";
}

int seconds_field(const json& object, const char* name, int fallback = 0) {
    const auto it = object.find(name);
    if (it == object.end() || !it->is_number_integer()) return fallback;
    const auto n = it->get<int64_t>();
    return n > 0 && n <= std::numeric_limits<int>::max() ? static_cast<int>(n) : fallback;
}

std::string encode(const std::string& value) {
    static constexpr char hex[] = "0123456789ABCDEF";
    std::string out;
    for (unsigned char c : value) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '.' || c == '_' || c == '~') {
            out += c;
        } else {
            out += '%';
            out += hex[c >> 4];
            out += hex[c & 15];
        }
    }
    return out;
}

std::string google_error(const std::string& code) {
    if (code == "access_denied") return ui("Accesso annullato");
    if (code == "expired_token") return ui("Codice scaduto");
    if (code == "invalid_client" || code == "unauthorized_client")
        return ui("Client Google non valido. ID e secret devono appartenere allo stesso client TV/dispositivi con input limitato.");
    if (code == "invalid_grant") return ui("Accesso Google scaduto o revocato. Disconnetti e accedi di nuovo.");
    return ui("Accesso non riuscito. Controlla la connessione e riprova.");
}

std::optional<GoogleTokens> parse_tokens(const json& data, const std::string& client_id,
                                         const std::string& previous_refresh = {}) {
    GoogleTokens tokens;
    tokens.client_id = client_id;
    tokens.refresh_token = field(data, "refresh_token");
    if (tokens.refresh_token.empty()) tokens.refresh_token = previous_refresh;
    tokens.access_token = field(data, "access_token");
    auto type = field(data, "token_type");
    std::transform(type.begin(), type.end(), type.begin(), [](unsigned char c) { return std::tolower(c); });
    const int lifetime = seconds_field(data, "expires_in");
    if (type != "bearer" || !lifetime || tokens.refresh_token.empty() || tokens.access_token.empty() ||
        tokens.access_token.find_first_of("\r\n") != std::string::npos ||
        tokens.access_token.find('\0') != std::string::npos) return std::nullopt;
    if (data.contains("scope")) {
        std::istringstream scopes(field(data, "scope"));
        std::string granted;
        while (scopes >> granted && granted != scope) {}
        if (granted != scope) return std::nullopt;
    }
    tokens.expires_at = Clock::now() + std::chrono::seconds(lifetime);
    return tokens;
}

std::optional<std::string> access_token(HttpClient& http, AuthStore& store, std::string& error,
                                        const std::string& rejected = {}) {
    // Serialize refreshes. Account changes during the network call are checked at commit.
    static std::mutex refresh_mutex;
    std::lock_guard<std::mutex> lock(refresh_mutex);
    const auto session = store.session();
    if (!session.device_login()) { error = ui("Accedi al tuo account Google"); return std::nullopt; }
    const auto& old = session.google;
    if (!old.access_token.empty() && old.access_token != rejected &&
        Clock::now() + std::chrono::seconds(30) < old.expires_at) return old.access_token;
    const auto client = load_google_client(error);
    if (!client) return std::nullopt;
    if (client->id != old.client_id) {
        error = ui("Credenziali Google non valide. Controlla la configurazione.");
        return std::nullopt;
    }
    const auto body = "grant_type=refresh_token&client_id=" + encode(client->id) +
        "&client_secret=" + encode(client->secret) + "&refresh_token=" + encode(old.refresh_token);
    const auto response = http.request(token_url, form_headers, &body, false);
    if (!response || response->status >= 500) {
        error = ui("Rinnovo temporaneamente non disponibile. Account conservato: riprova con la rete attiva.");
        return std::nullopt;
    }
    const auto data = json::parse(response->body, nullptr, false);
    if (!response->ok()) { error = google_error(field(data, "error")); return std::nullopt; }
    const auto tokens = parse_tokens(data, client->id, old.refresh_token);
    if (!tokens) { error = ui("Risposta Google incompleta"); return std::nullopt; }
    if (!store.save_google_tokens(*tokens, error, &old)) return std::nullopt;
    return tokens->access_token;
}
}  // namespace

std::string google_client_path() {
#ifdef __SWITCH__
    return "sdmc:/switch/KaiTube-Native/google-client.json";
#else
    return "google-client.json";
#endif
}

std::optional<GoogleClient> load_google_client(std::string& error) {
    std::ifstream file(google_client_path());
    if (file) {
        auto data = json::parse(file, nullptr, false);
        auto installed = data.find("installed");
        if (installed != data.end() && installed->is_object()) {
            GoogleClient client{field(*installed, "client_id"), field(*installed, "client_secret")};
            if (!client.id.empty() && !client.secret.empty()) return client;
        }
    }
    error = ui("Prima configura il client Google dell'app") + "\n" + google_client_path();
    return std::nullopt;
}

std::optional<DeviceCode> request_device_code(HttpClient& http, const GoogleClient& client, std::string& error) {
    const auto body = "client_id=" + encode(client.id) + "&scope=" + encode(scope);
    const auto response = http.request("https://oauth2.googleapis.com/device/code", form_headers, &body, false);
    if (!response) { error = ui("Connessione a Google non riuscita"); return std::nullopt; }
    const auto data = json::parse(response->body, nullptr, false);
    if (!response->ok()) { error = google_error(field(data, "error")); return std::nullopt; }
    DeviceCode result;
    result.device_code = field(data, "device_code");
    result.user_code = field(data, "user_code");
    result.verification_url = field(data, "verification_url");
    const int expires = seconds_field(data, "expires_in");
    if (result.device_code.empty() || result.user_code.empty() ||
        result.verification_url.rfind("https://", 0) != 0 || !expires) {
        error = ui("Risposta Google incompleta");
        return std::nullopt;
    }
    result.deadline = Clock::now() + std::chrono::seconds(expires);
    result.interval_seconds = seconds_field(data, "interval", 5);
    return result;
}

PollResult poll_device_code(HttpClient& http, const GoogleClient& client, const DeviceCode& code) {
    if (Clock::now() >= code.deadline) return {PollState::failed, {}, ui("Codice scaduto")};
    const auto body = "grant_type=urn%3Aietf%3Aparams%3Aoauth%3Agrant-type%3Adevice_code&client_id=" +
        encode(client.id) + "&client_secret=" + encode(client.secret) + "&device_code=" + encode(code.device_code);
    const auto response = http.request(token_url, form_headers, &body, false);
    if (!response || response->status >= 500) return {PollState::retry, {}, {}};
    const auto data = json::parse(response->body, nullptr, false);
    if (!response->ok()) {
        const auto reason = field(data, "error");
        if (reason == "authorization_pending") return {PollState::pending, {}, {}};
        if (reason == "slow_down" || response->status == 429) return {PollState::slow_down, {}, {}};
        return {PollState::failed, {}, google_error(reason)};
    }
    const auto tokens = parse_tokens(data, client.id);
    if (!tokens) return {PollState::failed, {}, ui("Risposta Google incompleta")};
    return {PollState::granted, *tokens, {}};
}

std::optional<HomeFeed> google_subscriptions(HttpClient& http, AuthStore& store, std::string& error) {
    auto token = access_token(http, store, error);
    if (!token) return std::nullopt;
    HomeFeed feed;
    feed.kiosk = {"google_subscriptions", "YouTube"};
    std::unordered_set<std::string> seen, pages;
    std::string page;
    do {
        if (http_cancel && http_cancel->load()) return std::nullopt;
        std::string url = "https://www.googleapis.com/youtube/v3/subscriptions?part=snippet&mine=true&maxResults=50&order=alphabetical";
        if (!page.empty()) url += "&pageToken=" + encode(page);
        auto get_page = [&] { return http.request(url, {{"Authorization", "Bearer " + *token}}, nullptr, false); };
        auto response = get_page();
        if (response && response->status == 401) {
            token = access_token(http, store, error, *token);
            if (!token) return std::nullopt;
            response = get_page();
        }
        if (!response || response->status >= 500) {
            error = ui("Connessione a Google non riuscita");
            return std::nullopt;
        }
        if (!response->ok()) {
            const auto data = json::parse(response->body, nullptr, false);
            const auto detail = data.find("error");
            error = detail == data.end() ? "" : field(*detail, "message");
            if (response->status == 401) error = google_error("invalid_grant");
            if (error.empty()) error = ui("Risposta Google incompleta");
            return std::nullopt;
        }
        const auto data = json::parse(response->body, nullptr, false);
        const auto items = data.find("items");
        if (items == data.end() || !items->is_array()) {
            error = ui("Risposta Google incompleta");
            return std::nullopt;
        }
        for (const auto& entry : *items) {
            const auto snippet = entry.find("snippet");
            if (snippet == entry.end()) continue;
            const auto resource = snippet->find("resourceId");
            if (resource == snippet->end()) continue;
            StreamItem channel;
            channel.channel_id = field(*resource, "channelId");
            channel.title = field(*snippet, "title");
            if (channel.channel_id.empty() || channel.title.empty() || !seen.insert(channel.channel_id).second) continue;
            channel.channel_url = "https://www.youtube.com/channel/" + channel.channel_id;
            channel.url = channel.channel_url;
            const auto images = snippet->find("thumbnails");
            if (images != snippet->end()) {
                for (const auto* size : {"medium", "default", "high"}) {
                    const auto img = images->find(size);
                    if (img != images->end()) channel.thumbnail_url = field(*img, "url");
                    if (!channel.thumbnail_url.empty()) break;
                }
            }
            feed.items.push_back(std::move(channel));
            if (feed.items.size() == 96) break;
        }
        page = field(data, "nextPageToken");
    } while (!page.empty() && pages.insert(page).second && pages.size() <= 7 && feed.items.size() < 96);
    error.clear();
    return feed;
}
}  // namespace newpipe
