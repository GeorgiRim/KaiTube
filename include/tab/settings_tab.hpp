#pragma once

#include <borealis.hpp>

#include "newpipe/http_proxy.hpp"

class SettingsTab : public brls::Box {
public:
    SettingsTab();

    static brls::View* create() { return new SettingsTab(); }

private:
    void syncLocalizedText();
    void syncFromStore();
    void resetToDefaults();
    void openSessionInfo();
    void editProxy();
    void editProxyLogin();
    void saveProxy(const newpipe::HttpProxy& proxy);

    BRLS_BIND(brls::SelectorCell, languageCell, "settings/language");
    BRLS_BIND(brls::SelectorCell, playbackQualityCell, "settings/playback_quality");
    BRLS_BIND(brls::SelectorCell, startupTabCell, "settings/startup_tab");
    BRLS_BIND(brls::SelectorCell, homeKioskCell, "settings/home_kiosk");
    BRLS_BIND(brls::BooleanCell, hideShortsCell, "settings/hide_shorts");
    BRLS_BIND(brls::DetailCell, proxyCell, "settings/http_proxy");
    BRLS_BIND(brls::DetailCell, proxyLoginCell, "settings/proxy_login");
    BRLS_BIND(brls::DetailCell, sessionCell, "settings/session");
};
