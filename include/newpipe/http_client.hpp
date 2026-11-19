#pragma once

#include <optional>
#include <atomic>
#include <string>
#include <vector>
#include <utility>

namespace newpipe {

inline std::atomic_bool ui_requests_cancel{false};
inline thread_local const std::atomic_bool* http_cancel = nullptr;
struct HttpCancelScope {
 const std::atomic_bool* previous;
 explicit HttpCancelScope(const std::atomic_bool* flag):previous(http_cancel){http_cancel=flag;}
 ~HttpCancelScope(){http_cancel=previous;}
};

struct HttpHeader {
    std::string name;
    std::string value;
};

struct HttpResponse {
    long status;
    std::string body;

    bool ok() const { return status >= 200 && status < 300; }
};

class HttpClient {
public:
    virtual ~HttpClient() = default;

    virtual std::optional<HttpResponse> request(
        const std::string& url, const std::vector<HttpHeader>& headers,
        const std::string* post_body, bool follow_redirects) = 0;

    std::optional<std::string> get(const std::string& url, const std::vector<HttpHeader>& headers = {}) {
        auto response = request(url, headers, nullptr, true);
        if (!response || !response->ok()) return std::nullopt;
        return std::move(response->body);
    }

    std::optional<std::string> post(const std::string& url, const std::string& body,
                                    const std::vector<HttpHeader>& headers = {}) {
        auto response = request(url, headers, &body, true);
        if (!response || !response->ok()) return std::nullopt;
        return std::move(response->body);
    }
};

class HttpsHttpClient final : public HttpClient {
public:
    std::optional<HttpResponse> request(
        const std::string& url, const std::vector<HttpHeader>& headers,
        const std::string* post_body, bool follow_redirects) override;
};

}  // namespace newpipe
