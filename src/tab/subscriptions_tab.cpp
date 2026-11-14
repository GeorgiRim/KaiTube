#include "newpipe/ui_text.hpp"
#include "tab/subscriptions_tab.hpp"
#include "activity/google_login.hpp"

#include "activity/stream_detail_activity.hpp"
#include "newpipe/auth_store.hpp"
#include "newpipe/i18n.hpp"
#include "newpipe/library_store.hpp"
#include "newpipe/log.hpp"
#include "newpipe/playback_helper.hpp"
#include "newpipe/runtime.hpp"
#include "view/stream_card.hpp"
#include "view/tab_focus.hpp"

namespace {
constexpr size_t kGridColumns = 3;
std::vector<newpipe::StreamItem> cached_items;
bool cached_cloud=false,cached_favorites=false;
}

SubscriptionsTab::SubscriptionsTab() : service_() {
  this->inflateFromXMLRes("xml/tabs/subscriptions.xml");
  newpipe::log_line("subscriptions: construct");
  brls::delay(700, [this, life = life_]() {
    if (life->alive)
      interactionReady_.store(true);
  });

  this->refresh();
}

void SubscriptionsTab::onCreate() {
  // Mirrored on the sidebar item: while signed out this tab has no focusable
  // child, so a content-only action could never be triggered.
  this->registerTabAction(newpipe::tr("common/refresh"),
                          brls::ControllerButton::BUTTON_X,
                          [this](brls::View *) {
                            this->refresh();
                            return true;
                          });
  this->registerTabAction(newpipe::tr("subscriptions/session_action"),
                          brls::ControllerButton::BUTTON_RB,
                          [this](brls::View *) {
                            this->openSessionDialog();
                            return true;
                          });
}

bool SubscriptionsTab::allowInitialInput() const {
  return interactionReady_.load();
}

void SubscriptionsTab::refresh() {
  if(newpipe::restore_video_focus && !newpipe::restore_activity && newpipe::ui_tab==2 && !cached_items.empty()) {
    items_=cached_items;interactionReady_=true;buildGrid();
    if(spinner)spinner->setVisibility(brls::Visibility::GONE);return;
  }
  newpipe::log_line("subscriptions: refresh");
  if (this->spinner) {
    this->spinner->setVisibility(brls::Visibility::VISIBLE);
  }

  std::string auth_error;
  if (!this->service_.load_auth_session(&auth_error)) {
    if (this->statusLabel) {
      this->statusLabel->setText(
          auth_error.empty() ? newpipe::tr("subscriptions/session_load_failed")
                             : auth_error);
    }
    if (this->bodyLabel) {
      this->bodyLabel->setText(
          newpipe::tr("subscriptions/session_load_failed_body"));
    }
    this->clearGrid();
    if (this->spinner) {
      this->spinner->setVisibility(brls::Visibility::GONE);
    }
    return;
  }

  if (!this->service_.has_auth_session()) {
    this->showSignedOutState();
    if (this->spinner) {
      this->spinner->setVisibility(brls::Visibility::GONE);
    }
    return;
  }

  newpipe::ui_job(
      life_,
      []() {
        newpipe::YouTubeCatalogService service;
        auto feed = service.get_subscriptions_feed();
        return std::make_pair(feed, service.error_message());
      },
      [this](auto result, const std::string &exception) {
        const auto &feed = result.first;
        const std::string request_error =
            exception.empty() ? result.second : exception;
        if (!feed.has_value()) {
          if (this->statusLabel) {
            this->statusLabel->setText(
                request_error.empty()
                    ? newpipe::tr("subscriptions/feed_load_failed")
                    : request_error);
          }
          const auto session = this->service_.auth_session();
          if (this->bodyLabel) {
            std::string body =
                newpipe::tr("subscriptions/feed_load_failed_body");
            if (!session.source_path.empty()) {
              body += "\n\n" + newpipe::tr("subscriptions/current_source",
                                           session.source_path);
            }
            this->bodyLabel->setText(body);
          }
          this->clearGrid();
          if (this->spinner) {
            this->spinner->setVisibility(brls::Visibility::GONE);
          }
          return;
        }

        this->items_ = feed->items;
        this->buildGrid();

        if (this->statusLabel) {
          this->statusLabel->setText(newpipe::tr("common/count_with_title",
                                                 feed->kiosk.title,
                                                 this->items_.size()));
        }
        if (this->bodyLabel) {
          const auto session = this->service_.auth_session();
          std::string session_name;
          if (!session.display_name.empty()) {
            session_name = session.display_name;
          } else if (!session.source_path.empty()) {
            session_name = session.source_path;
          } else {
            session_name = newpipe::tr("settings/session/saved");
          }
          std::string body =
              newpipe::tr("subscriptions/session_prefix", session_name);
          body += "\n" + newpipe::tr("subscriptions/controls");
          this->bodyLabel->setText(body);
        }
        if (this->spinner) {
          this->spinner->setVisibility(brls::Visibility::GONE);
        }
      });
}

void SubscriptionsTab::buildGrid() {
  cached_items=items_;
  if (!this->gridBox) {
    return;
  }

  newpipe::release_grid_focus(this, this->gridBox);
  this->gridBox->clearViews();
  for (size_t i = 0; i < this->items_.size(); i += kGridColumns) {
    auto *row = new brls::Box(brls::Axis::ROW);
    row->setMarginBottom(18);

    for (size_t j = i; j < i + kGridColumns && j < this->items_.size(); j++) {
      auto *card = new StreamCard();
      card->setData(this->items_[j]);
      const size_t idx = j;
      card->registerClickAction([this, idx](brls::View *) {
        this->playStream(this->items_[idx]);
        return true;
      });
      card->registerAction(newpipe::tr("common/info"),
                           brls::ControllerButton::BUTTON_Y,
                           [this, idx](brls::View *) {
                             this->openStream(this->items_[idx]);
                             return true;
                           });
      card->addGestureRecognizer(new brls::TapGestureRecognizer(card));
      row->addView(card);
    }

    this->gridBox->addView(row);
  }
  newpipe::wire_grid_navigation(gridBox);
  if(newpipe::restore_video_focus && !newpipe::restore_activity && newpipe::ui_tab==2) {
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

void SubscriptionsTab::clearGrid() {
  this->items_.clear();
  if (this->gridBox) {
    newpipe::release_grid_focus(this, this->gridBox);
    this->gridBox->clearViews();
  }
}

void SubscriptionsTab::showSignedOutState() {
  ++life_->generation;
  this->clearGrid();
  if (this->statusLabel) {
    this->statusLabel->setText(newpipe::tr("subscriptions/signed_out_title"));
  }
  if (this->bodyLabel) {
    const std::string body =
        newpipe::ui("Collega Google con un codice sul telefono per vedere i nuovi video delle tue iscrizioni. Premi R per accedere.");
    this->bodyLabel->setText(body);
  }
}

void SubscriptionsTab::openSessionDialog() { show_google_login(); }

void SubscriptionsTab::handleManualCookieInput(const std::string &text) {
  if (text.empty()) {
    brls::Application::notify(
        newpipe::tr("subscriptions/session_dialog/empty_input"));
    return;
  }

  std::string error;
  if (this->service_.update_auth_session_from_cookie(text, "manual input",
                                                     &error)) {
    brls::Application::notify(
        newpipe::tr("subscriptions/session_dialog/save_cookie_done"));
  } else {
    brls::Application::notify(
        error.empty()
            ? newpipe::tr("subscriptions/session_dialog/save_cookie_failed")
            : error);
  }
  this->refresh();
}

void SubscriptionsTab::playStream(const newpipe::StreamItem &item) {
  if (!allowInitialInput()) {
    return;
  }
  newpipe::logf("subscriptions: playStream url=%s", item.url.c_str());
  const std::optional<newpipe::StreamDetail> detail;
  const auto request = newpipe::build_playback_request(item, detail);
  if (!request.has_value()) {
    this->openStream(item);
    return;
  }

  std::string ignored_error;
  newpipe::LibraryStore::instance().add_history(
      detail.has_value() ? detail->item : item, &ignored_error);
  newpipe::restore_activity={};
  newpipe::request_playback(*request);
}

void SubscriptionsTab::openStream(const newpipe::StreamItem &item) {
  if (!allowInitialInput()) {
    return;
  }
  brls::Application::pushActivity(new StreamDetailActivity(item));
}
