#pragma once

#include <optional>
#include <string>

namespace newpipe {

// An empty host uses the application's default network connection.
struct HttpProxy {
    std::string host;
    int port = 0;
    std::string username;
    std::string password;
    static constexpr size_t max_credential_bytes = 128;

    static std::optional<HttpProxy> parse(const std::string& address);
    bool set_credentials(const std::string& user, const std::string& pass);
    std::string url() const;
    std::string ffmpeg_url() const;
};

}  // namespace newpipe
