#pragma once

#include <curl/curl.h>
#include "newpipe/settings_store.hpp"

namespace newpipe {

inline CURL* create_curl_handle() {
    CURL* curl = curl_easy_init();
    if (!curl) return nullptr;

    const auto proxy = SettingsStore::instance().settings().http_proxy.url();
    if (!proxy.empty() &&
        (curl_easy_setopt(curl, CURLOPT_PROXY, proxy.c_str()) != CURLE_OK ||
         curl_easy_setopt(curl, CURLOPT_NOPROXY, "") != CURLE_OK)) {
        curl_easy_cleanup(curl);
        return nullptr;
    }
    return curl;
}

}  // namespace newpipe
