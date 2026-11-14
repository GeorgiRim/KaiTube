#include "tab/home_tab.hpp"

#include "activity/stream_detail_activity.hpp"
#include "newpipe/i18n.hpp"
#include "newpipe/library_store.hpp"
#include "newpipe/log.hpp"
#include "newpipe/playback_helper.hpp"
#include "newpipe/runtime.hpp"
#include "newpipe/settings_store.hpp"
#include "view/stream_card.hpp"
#include "view/tab_focus.hpp"

namespace {
constexpr size_t kGridColumns = 3;
std::vector<newpipe::StreamItem> cached_items;
std::string cached_query;
}

HomeTab::HomeTab() : service_() {
  this->inflateFromXMLRes("xml/tabs/home.xml");
  newpipe::log_line("home: construct");

  kiosks_ = service_.list_kiosks();
  newpipe::logf("home: kiosks=%zu", kiosks_.size());
  const newpipe::AppSettings settings =
      newpipe::SettingsStore::instance().settings();
  for (size_t i = 0; i < kiosks_.size(); i++) {
    if (kiosks_[i].id == settings.home_kiosk) {
      kioskIndex_ = i;
      break;
    }
  }

  if (statusLabel) {
    statusLabel->setText(newpipe::tr("home/preparing"));
  }
  brls::delay(700, [this, life = life_]() {
    if (!life->alive)
      return;
    interactionReady_.store(true);
    newpipe::log_line("home: interaction ready");
  });
  scheduleLoadHome(250);
}

void HomeTab::onCreate() {
  // Tab actions are mirrored on the sidebar item so they stay usable while the
  // sidebar holds focus, and when the feed is empty and nothing here is
  // focusable.
  this->registerTabAction(newpipe::tr("common/refresh"),
                          brls::ControllerButton::BUTTON_X,
                          [this](brls::View *) {
                            loadHome(true);
                            return true;
                          });
  this->registerTabAction(newpipe::tr("home/category_action"),
                          brls::ControllerButton::BUTTON_Y,
                          [this](brls::View *) {
                            cycleKiosk();
                            return true;
                          });
}

void HomeTab::loadHome(bool forceRefresh) {
  if (forceRefresh) {
    newpipe::restore_video_focus = false;
    newpipe::log_line("home: explicit refresh, requesting fresh feed");
  }
  if (!forceRefresh && newpipe::restore_video_focus && newpipe::ui_tab==0 && !cached_items.empty()) {
    items_=cached_items;initialLoadCompleted_=true;interactionReady_=true;buildGrid();
    if(spinner)spinner->setVisibility(brls::Visibility::GONE);return;
  }
  if(sourceLabel)sourceLabel->setText(newpipe::tr("settings/home_kiosk/options/recommended"));
  newpipe::logf("home: loadHome index=%zu", kioskIndex_);
  if (!initialLoadCompleted_) {
    initialLoadAttempts_++;
  }
  if (spinner) {
    spinner->setVisibility(brls::Visibility::VISIBLE);
  }

  if (!service_.is_loaded()) {
    if (statusLabel) {
      statusLabel->setText(
          newpipe::tr("common/service_init_failed", service_.error_message()));
    }
    if (spinner) {
      spinner->setVisibility(brls::Visibility::GONE);
    }
    return;
  }

  if (kiosks_.empty()) {
    if (statusLabel) {
      statusLabel->setText(newpipe::tr("home/no_kiosk"));
    }
    if (spinner) {
      spinner->setVisibility(brls::Visibility::GONE);
    }
    return;
  }

  kioskIndex_ %= kiosks_.size();
  const std::string kiosk = kiosks_[kioskIndex_].id;
  newpipe::ui_job(
      life_,
      [kiosk]() {
        newpipe::YouTubeCatalogService service;
        auto feed = service.get_home_feed(kiosk);
        return std::make_pair(feed, service.error_message());
      },
      [this](auto result, const std::string &exception) {
        const auto &feed = result.first;
        const std::string request_error =
            exception.empty() ? result.second : exception;
        if (!feed.has_value()) {
          if (statusLabel) {
            if (request_error.empty()) {
              statusLabel->setText(newpipe::tr("home/load_failed"));
            } else {
              statusLabel->setText(request_error);
            }
          }
          if (spinner) {
            spinner->setVisibility(brls::Visibility::GONE);
          }
          if (!initialLoadCompleted_ && items_.empty() &&
              initialLoadAttempts_ < 4) {
            if (statusLabel) {
              statusLabel->setText(newpipe::tr("home/preparing"));
            }
            newpipe::logf("home: auto retry attempt=%d", initialLoadAttempts_);
            scheduleLoadHome(350);
          }
          return;
        }

        initialLoadCompleted_ = true;
        items_ = feed->items;
        cached_items=items_;
        newpipe::logf("home: feed=%s items=%zu", feed->kiosk.id.c_str(),
                      items_.size());
        buildGrid();

        if (statusLabel) {
          std::string title = feed->kiosk.title;
          const std::string option_key =
              "settings/home_kiosk/options/" + feed->kiosk.id;
          const std::string translated = newpipe::tr(option_key);
          if (feed->kiosk.id!="recommended" && !translated.empty() && translated != option_key) {
            title = translated;
          }
          statusLabel->setText(
              newpipe::tr("common/count_with_title", title, items_.size()));
        }
        if (spinner) {
          spinner->setVisibility(brls::Visibility::GONE);
        }
      });
}

void HomeTab::scheduleLoadHome(long delay_ms) {
  brls::delay(delay_ms, [this, life = life_]() {
    if (life->alive)
      loadHome();
  });
}

void HomeTab::buildGrid() {
  if (!gridBox) {
    newpipe::log_line("home: buildGrid gridBox missing");
    return;
  }

  newpipe::logf("home: buildGrid items=%zu", items_.size());
  newpipe::release_grid_focus(this, gridBox);
  gridBox->clearViews();
  for (size_t i = 0; i < items_.size(); i += kGridColumns) {
    auto *row = new brls::Box(brls::Axis::ROW);
    row->setMarginBottom(18);

    for (size_t j = i; j < i + kGridColumns && j < items_.size(); j++) {
      auto *card = new StreamCard();
      card->setData(items_[j]);
      const size_t idx = j;
      card->registerClickAction([this, idx](brls::View *) {
        playStream(items_[idx]);
        return true;
      });
      card->registerAction(newpipe::tr("common/info"),
                           brls::ControllerButton::BUTTON_Y,
                           [this, idx](brls::View *) {
                             openStream(items_[idx]);
                             return true;
                           });
      card->addGestureRecognizer(new brls::TapGestureRecognizer(card));
      row->addView(card);
    }

    gridBox->addView(row);
  }
  newpipe::wire_grid_navigation(gridBox);
  if(newpipe::restore_video_focus && !newpipe::restore_activity && newpipe::ui_tab==0) {
    for(auto* rowView:gridBox->getChildren()) {
      auto* row=dynamic_cast<brls::Box*>(rowView); if(!row)continue;
      for(auto* cardView:row->getChildren()) {
        auto* card=dynamic_cast<StreamCard*>(cardView);
        if(card && card->video_url_==newpipe::focused_video) {
          newpipe::restore_video_focus=false;
          brls::delay(100,[this,life=life_,generation=life_->generation.load(),card]{if(life->alive && life->generation==generation)brls::Application::giveFocus(card);});
          return;
        }
      }
    }
  }

}

void HomeTab::cycleKiosk() {
  if (!allowInitialInput()) {
    return;
  }
  if (kiosks_.empty()) {
    return;
  }

  kioskIndex_ = (kioskIndex_ + 1) % kiosks_.size();
  newpipe::logf("home: cycleKiosk newIndex=%zu id=%s", kioskIndex_,
                kiosks_[kioskIndex_].id.c_str());
  loadHome(true);
}

bool HomeTab::allowInitialInput() const {
  if (interactionReady_.load()) {
    return true;
  }

  newpipe::log_line("home: ignored startup input");
  return false;
}

void HomeTab::playStream(const newpipe::StreamItem &item) {
  if (!allowInitialInput()) {
    return;
  }
  newpipe::logf("home: playStream url=%s", item.url.c_str());
  const std::optional<newpipe::StreamDetail> detail;
  const auto request = newpipe::build_playback_request(item, detail);
  if (!request.has_value()) {
    openStream(item);
    return;
  }

  std::string ignored_error;
  newpipe::LibraryStore::instance().add_history(
      detail.has_value() ? detail->item : item, &ignored_error);
  newpipe::logf("home: queue playback url=%s", request->url.c_str());
  newpipe::restore_activity={};
  newpipe::request_playback(*request);
}

void HomeTab::openStream(const newpipe::StreamItem &item) {
  if (!allowInitialInput()) {
    return;
  }
  newpipe::logf("home: openStream url=%s", item.url.c_str());
  brls::Application::pushActivity(new StreamDetailActivity(item));
}
