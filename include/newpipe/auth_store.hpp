#pragma once

#include <mutex>
#include <chrono>
#include <string>
#include <vector>

#include "newpipe/http_client.hpp"

namespace newpipe {

struct GoogleTokens {
    std::string client_id;
    std::string refresh_token;
    std::string access_token;
    std::chrono::steady_clock::time_point expires_at;
};

struct AuthSession {
    std::string cookie_header;
    std::string sapisid;
    std::string source_label;
    std::string source_path;
    std::string display_name;

    GoogleTokens google;
    bool has_cookies() const { return !cookie_header.empty() && !sapisid.empty(); }
    bool device_login() const { return !google.client_id.empty() && !google.refresh_token.empty(); }
    bool authenticated() const { return has_cookies() || device_login(); }
};

std::string default_auth_import_path();
std::string default_auth_session_path();

class AuthStore {
public:
    static AuthStore& instance();

    bool load(std::string* error_message = nullptr);
    bool reload(std::string* error_message = nullptr);

    AuthSession session() const;
    bool has_session() const;

    bool update_from_cookie_header(
        const std::string& cookie_header,
        const std::string& source_label,
        std::string* error_message = nullptr);
    bool import_from_file(
        const std::string& file_path = {},
        std::string* error_message = nullptr);
    bool clear(std::string* error_message = nullptr);
    bool save_google_tokens(const GoogleTokens& tokens, std::string& error,
                            const GoogleTokens* previous = nullptr);

    std::vector<HttpHeader> build_youtube_headers(
        const std::string& origin,
        const std::string& referer,
        std::string* error_message = nullptr) const;

private:
    AuthStore() = default;

    bool persist_session(std::string* error_message);

    mutable std::mutex mutex_;
    bool loaded_ = false;
    AuthSession session_;
};

}  // namespace newpipe
