#pragma once

#include <optional>
#include <string>

namespace newpipe {

// An empty host uses the application's default network connection.
struct HttpProxy {
    std::string host;
    int port = 0;

    static std::optional<HttpProxy> parse(const std::string& address);
    std::string url() const;
};

}  // namespace newpipe
