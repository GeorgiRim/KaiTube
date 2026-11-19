#pragma once

#include <curl/curl.h>
#include "newpipe/settings_store.hpp"

namespace newpipe {

inline CURL* create_curl_handle() {
    CURL* curl = curl_easy_init();
    if (!curl) return nullptr;

    const auto proxy = SettingsStore::instance().settings().http_proxy;
    const auto url = proxy.url();
    if (!url.empty() &&
        (curl_easy_setopt(curl, CURLOPT_PROXY, url.c_str()) != CURLE_OK ||
         curl_easy_setopt(curl, CURLOPT_NOPROXY, "") != CURLE_OK)) {
        curl_easy_cleanup(curl);
        return nullptr;
    }
    if (!proxy.username.empty() &&
        (curl_easy_setopt(curl, CURLOPT_PROXYAUTH, static_cast<long>(CURLAUTH_BASIC)) != CURLE_OK ||
         curl_easy_setopt(curl, CURLOPT_PROXYUSERNAME, proxy.username.c_str()) != CURLE_OK ||
         curl_easy_setopt(curl, CURLOPT_PROXYPASSWORD, proxy.password.c_str()) != CURLE_OK)) {
        curl_easy_cleanup(curl);
        return nullptr;
    }
    return curl;
}

}  // namespace newpipe
