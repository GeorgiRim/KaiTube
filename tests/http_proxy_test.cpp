#include "newpipe/settings_store.hpp"

#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>

int main() {
    using newpipe::HttpProxy;
    assert(HttpProxy::parse("")->url().empty());
    assert(HttpProxy::parse("192.168.1.10:8080")->url() == "http://192.168.1.10:8080");
    assert(HttpProxy::parse("http://proxy.example:65535/")->host == "proxy.example");
    assert(HttpProxy::parse("proxy:1")->port == 1);
    for (const char* invalid : {"socks5://proxy:1080", "https://proxy:443", "user@host:80",
                                "host", "host:0", "host:65536", "host:80/path", "a b:80",
                                "[::1]:8080", "host:80?query", "host:80#frag"}) {
        assert(!HttpProxy::parse(invalid));
    }

    char temp_dir[] = "/tmp/kaitube-proxy-test-XXXXXX";
    if (!mkdtemp(temp_dir)) return 1;
    const auto original_dir = std::filesystem::current_path();
    std::filesystem::current_path(temp_dir);
    const auto path = newpipe::default_settings_store_path();
    std::ofstream(path) << R"({"language":"fr","startup_tab":"search"})";
    auto& store = newpipe::SettingsStore::instance();
    assert(store.settings().http_proxy.host.empty());
    const auto proxy = *HttpProxy::parse("http://proxy.example:3128");
    assert(store.update_http_proxy(proxy));
    assert(store.reload());
    assert(store.settings().http_proxy.url() == proxy.url());
    assert(store.settings().language == "fr" && store.settings().startup_tab == "search");

    std::filesystem::remove(path);
    std::filesystem::create_directory(path);
    assert(!store.update_http_proxy(HttpProxy{}));
    assert(store.settings().http_proxy.url() == proxy.url());
    std::filesystem::remove(path);

    assert(store.update_http_proxy(HttpProxy{}));
    assert(store.reload() && store.settings().http_proxy.host.empty());
    assert(store.update_http_proxy(proxy));
    assert(store.reset() && store.reload() && store.settings().http_proxy.host.empty());
    std::filesystem::current_path(original_dir);
    std::filesystem::remove_all(temp_dir);
}
