#include "newpipe/auth_store.hpp"
#include "tab/settings_tab.hpp"
#include "activity/google_login.hpp"
#include "newpipe/runtime.hpp"

#include "newpipe/auth_store.hpp"
#include "newpipe/i18n.hpp"
#include "newpipe/library_store.hpp"
#include "newpipe/log.hpp"
#include "newpipe/settings_store.hpp"

namespace {

const std::vector<std::string> language_codes={"auto","it","en-US","fr","de","es","pt","ko"};
int language_selection(const std::string& value) {
 auto i=std::find(language_codes.begin(),language_codes.end(),value);
 return i==language_codes.end()?0:static_cast<int>(i-language_codes.begin());
}
std::string language_from_selection(int selection) {
 return selection>=0 && selection<(int)language_codes.size()?language_codes[selection]:"auto";
}

int playback_quality_selection(newpipe::PlaybackQualityMode mode) {
    switch (mode) {
        case newpipe::PlaybackQualityMode::BEST:
            return 0;
        case newpipe::PlaybackQualityMode::HD_1080:
            return 1;
        case newpipe::PlaybackQualityMode::HD_720:
            return 2;
        case newpipe::PlaybackQualityMode::LOW_320:
            return 3;
        default:
            return 0;
    }
}

newpipe::PlaybackQualityMode playback_quality_from_selection(int selection) {
    switch (selection) {
        case 1:
            return newpipe::PlaybackQualityMode::HD_1080;
        case 2:
            return newpipe::PlaybackQualityMode::HD_720;
        case 3:
            return newpipe::PlaybackQualityMode::LOW_320;
        default:
            return newpipe::PlaybackQualityMode::BEST;
    }
}

int startup_tab_selection(const std::string& value) {
    if (value == "search") {
        return 1;
    }
    if (value == "subscriptions") {
        return 2;
    }
    if (value == "library") {
        return 3;
    }
    if (value == "settings") {
        return 4;
    }
    return 0;
}

std::string startup_tab_from_selection(int selection) {
    switch (selection) {
        case 1:
            return "search";
        case 2:
            return "subscriptions";
        case 3:
            return "library";
        case 4:
            return "settings";
        default:
            return "home";
    }
}

int home_kiosk_selection(const std::string& value) {
    if (value == "live") {
        return 1;
    }
    if (value == "music") {
        return 2;
    }
    if (value == "gaming") {
        return 3;
    }
    return 0;
}

std::string home_kiosk_from_selection(int selection) {
    switch (selection) {
        case 1:
            return "live";
        case 2:
            return "music";
        case 3:
            return "gaming";
        default:
            return "recommended";
    }
}

}  // namespace

SettingsTab::SettingsTab() {
    this->inflateFromXMLRes("xml/tabs/settings.xml");

    this->registerAction(newpipe::tr("settings/reset_action"), brls::ControllerButton::BUTTON_X, [this](brls::View*) {
        this->resetToDefaults();
        return true;
    });

    this->syncLocalizedText();

    this->languageCell->init(
        newpipe::tr("settings/language/title"),
        {
            newpipe::tr("settings/language/options/auto"),
            "Italiano", "English", "Français", "Deutsch", "Español", "Português", "한국어",
        },
        0,
        [this](int selection) {
            std::string error;
            if (!newpipe::SettingsStore::instance().update_language(
                    language_from_selection(selection), &error)) {
                brls::Application::notify(
                    error.empty() ? newpipe::tr("settings/language/save_failed") : error);
                this->syncFromStore();
                return;
            }
            newpipe::ui_reload=true; newpipe::ui_tab=4;
        });

    this->playbackQualityCell->init(
        newpipe::tr("settings/playback_quality/title"),
        {
            newpipe::tr("settings/playback_quality/options/best"),
            newpipe::tr("settings/playback_quality/options/hd_1080"),
            newpipe::tr("settings/playback_quality/options/hd_720"),
            newpipe::tr("settings/playback_quality/options/low_320"),
        },
        0,
        [this](int selection) {
            std::string error;
            if (!newpipe::SettingsStore::instance().update_playback_quality(
                    playback_quality_from_selection(selection), &error)) {
                brls::Application::notify(
                    error.empty() ? newpipe::tr("settings/playback_quality/save_failed") : error);
                this->syncFromStore();
                return;
            }
            brls::Application::notify(newpipe::tr("settings/playback_quality/saved"));
        });

    this->startupTabCell->init(
        newpipe::tr("settings/startup_tab/title"),
        {
            newpipe::tr("app/home"),
            newpipe::tr("app/search"),
            newpipe::tr("app/subscriptions"),
            newpipe::tr("app/library"),
            newpipe::tr("app/settings"),
        },
        0,
        [this](int selection) {
            std::string error;
            if (!newpipe::SettingsStore::instance().update_startup_tab(
                    startup_tab_from_selection(selection), &error)) {
                brls::Application::notify(
                    error.empty() ? newpipe::tr("settings/startup_tab/save_failed") : error);
                this->syncFromStore();
                return;
            }
            brls::Application::notify(newpipe::tr("settings/startup_tab/saved"));
        });

    this->homeKioskCell->init(
        newpipe::tr("settings/home_kiosk/title"),
        {
            newpipe::tr("settings/home_kiosk/options/recommended"),
            newpipe::tr("settings/home_kiosk/options/live"),
            newpipe::tr("settings/home_kiosk/options/music"),
            newpipe::tr("settings/home_kiosk/options/gaming"),
        },
        0,
        [this](int selection) {
            std::string error;
            if (!newpipe::SettingsStore::instance().update_home_kiosk(
                    home_kiosk_from_selection(selection), &error)) {
                brls::Application::notify(
                    error.empty() ? newpipe::tr("settings/home_kiosk/save_failed") : error);
                this->syncFromStore();
                return;
            }
            brls::Application::notify(newpipe::tr("settings/home_kiosk/saved"));
        });

    this->hideShortsCell->init(newpipe::tr("settings/hide_shorts/title"), false, [this](bool value) {
        std::string error;
        if (!newpipe::SettingsStore::instance().update_hide_short_videos(value, &error)) {
            brls::Application::notify(
                error.empty() ? newpipe::tr("settings/hide_shorts/save_failed") : error);
            this->syncFromStore();
            return;
        }
        brls::Application::notify(
            value ? newpipe::tr("settings/hide_shorts/enabled")
                  : newpipe::tr("settings/hide_shorts/disabled"));
    });

    this->proxyCell->setText(newpipe::tr("settings/http_proxy/title"));
    this->proxyCell->registerClickAction([this](brls::View*) {
        this->editProxy();
        return true;
    });

    this->sessionCell->setText(newpipe::tr("settings/session/title"));
    this->sessionCell->registerClickAction([this](brls::View*) {
        this->openSessionInfo();
        return true;
    });

    this->syncFromStore();
}

void SettingsTab::syncLocalizedText() {
    if (this->languageCell) {
        this->languageCell->setText(newpipe::tr("settings/language/title"));
    }
    if (this->playbackQualityCell) {
        this->playbackQualityCell->setText(newpipe::tr("settings/playback_quality/title"));
    }
    if (this->startupTabCell) {
        this->startupTabCell->setText(newpipe::tr("settings/startup_tab/title"));
    }
    if (this->homeKioskCell) {
        this->homeKioskCell->setText(newpipe::tr("settings/home_kiosk/title"));
    }
    if (this->hideShortsCell) {
        this->hideShortsCell->setText(newpipe::tr("settings/hide_shorts/title"));
    }
    if (this->sessionCell) {
        this->sessionCell->setText(newpipe::tr("settings/session/title"));
    }

}

void SettingsTab::syncFromStore() {
    std::string error;
    newpipe::SettingsStore::instance().load(&error);
    const newpipe::AppSettings settings = newpipe::SettingsStore::instance().settings();

    this->languageCell->setSelection(language_selection(settings.language), true);
    this->playbackQualityCell->setSelection(
        playback_quality_selection(settings.playback_quality), true);
    this->startupTabCell->setSelection(startup_tab_selection(settings.startup_tab), true);
    this->homeKioskCell->setSelection(home_kiosk_selection(settings.home_kiosk), true);
    this->hideShortsCell->setOn(settings.hide_short_videos, false);

    this->proxyCell->setDetailText(settings.http_proxy.host.empty()
        ? newpipe::tr("settings/http_proxy/off") : settings.http_proxy.url());
    this->sessionCell->setDetailText(newpipe::tr(newpipe::AuthStore::instance().has_session()
        ? "settings/session/saved" : "settings/session/signed_out"));


}

void SettingsTab::editProxy() {
    const auto current = newpipe::SettingsStore::instance().settings().http_proxy;
    brls::Application::getImeManager()->openForText(
        [this](std::string address) {
            const auto proxy = newpipe::HttpProxy::parse(address);
            if (!proxy) {
                brls::Application::notify(newpipe::tr("settings/http_proxy/invalid"));
                return;
            }
            std::string error;
            if (!newpipe::SettingsStore::instance().update_http_proxy(*proxy, &error)) {
                brls::Application::notify(error);
                return;
            }
            this->syncFromStore();
        },
        newpipe::tr("settings/http_proxy/title"), newpipe::tr("settings/http_proxy/hint"),
        270, current.url());
}

void SettingsTab::resetToDefaults() {
    std::string error;
    if (!newpipe::SettingsStore::instance().reset(&error)) {
        brls::Application::notify(error.empty() ? newpipe::tr("settings/reset_failed") : error);
        return;
    }

    newpipe::ui_reload=true;newpipe::ui_tab=4;
}

void SettingsTab::openSessionInfo() { show_google_login(); }

