#include "newpipe/http_proxy.hpp"

#include <algorithm>
#include <regex>

namespace newpipe {
namespace {

bool valid_credential(const std::string& value) {
    return value.size() <= HttpProxy::max_credential_bytes &&
        std::none_of(value.begin(), value.end(), [](unsigned char c) { return c < 32 || c == 127; });
}

std::string encode_userinfo(const std::string& text) {
    constexpr char hex[] = "0123456789ABCDEF";
    std::string result;
    for (unsigned char c : text) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '.' || c == '_' || c == '~') {
            result += static_cast<char>(c);
        } else {
            result += '%';
            result += hex[c >> 4];
            result += hex[c & 15];
        }
    }
    return result;
}

}  // namespace

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

    HttpProxy proxy;
    proxy.host = match[1].str();
    proxy.port = std::stoi(match[2].str());
    static const std::regex hostname(
        R"(^([a-zA-Z0-9]([a-zA-Z0-9-]{0,61}[a-zA-Z0-9])?\.)*[a-zA-Z0-9]([a-zA-Z0-9-]{0,61}[a-zA-Z0-9])?\.?$)");
    if (proxy.port == 0 || proxy.port > 65535 || proxy.host.size() > 253 ||
        !std::regex_match(proxy.host, hostname)) {
        return std::nullopt;
    }
    return proxy;
}

bool HttpProxy::set_credentials(const std::string& user, const std::string& pass) {
    if ((!user.empty() && host.empty()) || (user.empty() && !pass.empty()) ||
        user.find(':') != std::string::npos || !valid_credential(user) || !valid_credential(pass)) {
        return false;
    }
    username = user;
    password = pass;
    return true;
}

std::string HttpProxy::url() const {
    return host.empty() ? "" : "http://" + host + ":" + std::to_string(port);
}

std::string HttpProxy::ffmpeg_url() const {
    if (username.empty()) return url();
    return "http://" + encode_userinfo(username) + ":" + encode_userinfo(password) +
        "@" + host + ":" + std::to_string(port);
}

}  // namespace newpipe
