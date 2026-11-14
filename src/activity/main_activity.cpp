#include "newpipe/ui_text.hpp"
#include "activity/main_activity.hpp"

#include "newpipe/i18n.hpp"
#include "newpipe/runtime.hpp"
#include "newpipe/settings_store.hpp"

namespace {

size_t startup_tab_index(const std::string& tab_id) {
    if (tab_id == "search") {
        return 1;
    }
    if (tab_id == "subscriptions") {
        return 2;
    }
    if (tab_id == "library") {
        return 3;
    }
    if (tab_id == "settings") {
        return 4;
    }
    return 0;
}

}  // namespace

void MainActivity::onContentAvailable() {
    this->getContentView()->registerAction(newpipe::ui("Indietro"),brls::ControllerButton::BUTTON_B,[this](brls::View*){if(tabsFrame)tabsFrame->focusTab(static_cast<int>(tabsFrame->getActiveIndex()));return true;});
    this->registerAction(newpipe::ui("Indietro"), brls::ControllerButton::BUTTON_B,[this](brls::View*) {
      if(tabsFrame) tabsFrame->focusTab(static_cast<int>(tabsFrame->getActiveIndex()));
      return true;
    });
    this->registerAction(newpipe::ui("Esci"),brls::ControllerButton::BUTTON_START,[](brls::View*){
      auto* d=new brls::Dialog(newpipe::ui("Vuoi chiudere KaiTube?"));
      d->addButton(newpipe::ui("Annulla"),[d]{d->close();});
      d->addButton(newpipe::ui("Esci"),[]{brls::Application::quit();});d->open();return true;
    });
    this->registerAction(newpipe::tr("common/info"), brls::ControllerButton::BUTTON_Y, [this](brls::View*) {
        auto* dialog = new brls::Dialog(newpipe::tr("app/info_body", APP_VERSION));
        dialog->addButton(newpipe::tr("hints/ok"), [dialog]() { dialog->close(); });
        dialog->setCancelable(true);
        dialog->open();
        return true;
    });

    if (this->tabsFrame) {
        const size_t index = newpipe::ui_tab>=0 ? newpipe::ui_tab : startup_tab_index(newpipe::SettingsStore::instance().settings().startup_tab);
        tabsFrame->setTabChangedAction([](size_t i){newpipe::ui_tab=static_cast<int>(i);});
        this->tabsFrame->setDefaultTabIndex(index);
        this->tabsFrame->focusTab(static_cast<int>(index));
    }
}
