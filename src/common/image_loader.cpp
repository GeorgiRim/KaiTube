#include "newpipe/image_loader.hpp"

#include <chrono>
#include <regex>

#include "newpipe/http_client.hpp"
#include "newpipe/log.hpp"

namespace newpipe {

namespace {

// nanovg/stb_image can't decode WebP, so YouTube's vi_webp/*.webp thumbnails come
// back as a 0 texture. Rewrite to the equivalent JPG that stb_image can decode.
std::string rewrite_unsupported_image_url(const std::string& url) {
    static const std::regex video(R"(https://i\.ytimg\.com/vi(?:_webp)?/([A-Za-z0-9_-]{11})/)");
    std::smatch match;
    if (std::regex_search(url, match, video)) return "https://i.ytimg.com/vi/" + match[1].str() + "/mqdefault.jpg";

    std::string rewritten = url;
    const std::string webp_path = "/vi_webp/";
    const std::string jpg_path = "/vi/";
    auto pos = rewritten.find(webp_path);
    if (pos != std::string::npos) {
        rewritten.replace(pos, webp_path.size(), jpg_path);
    }
    const std::string webp_ext = ".webp";
    if (rewritten.size() >= webp_ext.size()) {
        const auto ext_pos = rewritten.rfind(webp_ext);
        if (ext_pos != std::string::npos) {
            const auto query_pos = rewritten.find('?', ext_pos);
            if (query_pos == std::string::npos
                && ext_pos + webp_ext.size() == rewritten.size()) {
                rewritten.replace(ext_pos, webp_ext.size(), ".jpg");
            }
        }
    }
    return rewritten;
}

}  // namespace

ImageLoader& ImageLoader::instance() {
    static ImageLoader loader;
    return loader;
}

ImageLoader::~ImageLoader() {
    stop();
}

void ImageLoader::start() {
    if (running_) {
        return;
    }

    cancelled_=false;
    running_ = true;
    log_line("image: start worker");
    thread_ = std::thread([this]() { worker(); });
}

void ImageLoader::stop() {
    cancelled_=true;
    running_ = false;
    if (thread_.joinable()) {
        thread_.join();
    }
    { std::lock_guard<std::mutex> lock(mutex_); while (!queue_.empty()) queue_.pop(); }
    log_line("image: stop worker");
}

std::string ImageLoader::cached_image(const std::string& url) {
 std::string bytes;tryGetCached(rewrite_unsupported_image_url(url),&bytes);return bytes;
}

void ImageLoader::load(const std::string& url, brls::Image* target) {
    if (!target) {
        return;
    }

    const std::string fetch_url = rewrite_unsupported_image_url(url);
    target->setImageAsync([fetch_url, this](std::function<void(const std::string&, size_t)> cb) {
        std::string cached;
        if (tryGetCached(fetch_url, &cached)) {
            logf("image: cache hit %s bytes=%zu", fetch_url.c_str(), cached.size());
            cb(cached, cached.size());
            return;
        }
        logf("image: queue %s", fetch_url.c_str());
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push({fetch_url, cb});
    });
}

bool ImageLoader::tryGetCached(const std::string& url, std::string* out) {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    auto it = cache_.find(url);
    if (it == cache_.end()) {
        return false;
    }
    *out = it->second;
    return true;
}

void ImageLoader::putCache(const std::string& url, const std::string& data) {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    size_t total = data.size();
    for (const auto& item : cache_) total += item.second.size();
    while (total > 8 * 1024 * 1024 && !cache_.empty()) {
        total -= cache_.begin()->second.size(); cache_.erase(cache_.begin());
    }
    cache_[url] = data;
}

void ImageLoader::worker() {
    HttpCancelScope cancel_scope(&cancelled_);
    HttpsHttpClient client;
    log_line("image: worker entered");

    while (running_) {
        AsyncRequest request;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!queue_.empty()) {
                request = std::move(queue_.front());
                queue_.pop();
            }
        }

        if (request.url.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        std::string cached;
        if (tryGetCached(request.url, &cached)) {
            logf("image: cache hit (worker) %s bytes=%zu", request.url.c_str(), cached.size());
            request.callback(cached, cached.size());
            continue;
        }

        auto data = client.get(request.url);
        if (data.has_value() && !data->empty()) {
            logf("image: fetched %s bytes=%zu", request.url.c_str(), data->size());
            putCache(request.url, *data);
            request.callback(*data, data->size());
        } else {
            logf("image: fetch failed %s", request.url.c_str());
            request.callback("", 0);
        }
    }

    log_line("image: worker exit");
}

}  // namespace newpipe
