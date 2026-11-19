#include "newpipe/http_proxy.hpp"

#include <regex>

namespace newpipe {

std::optional<HttpProxy> HttpProxy::parse(const std::string& address) {
    if (address.empty()) return HttpProxy{};

    // The Switch port of libcurl uses IPv4. Allow a DNS name or IPv4 address
    // and an explicit port, with the same HTTP URL syntax for all backends.
    static const std::regex endpoint(
        R"(^(?:http://)?([a-zA-Z0-9.-]+):([0-9]{1,5})/?$)");
    std::smatch match;
    if (address.size() > 270 || !std::regex_match(address, match, endpoint)) {
        return std::nullopt;
    }

    HttpProxy proxy{match[1].str(), std::stoi(match[2].str())};
    static const std::regex hostname(
        R"(^([a-zA-Z0-9]([a-zA-Z0-9-]{0,61}[a-zA-Z0-9])?\.)*[a-zA-Z0-9]([a-zA-Z0-9-]{0,61}[a-zA-Z0-9])?\.?$)");
    if (proxy.port == 0 || proxy.port > 65535 || proxy.host.size() > 253 ||
        !std::regex_match(proxy.host, hostname)) {
        return std::nullopt;
    }
    return proxy;
}

std::string HttpProxy::url() const {
    return host.empty() ? "" : "http://" + host + ":" + std::to_string(port);
}

}  // namespace newpipe
