#include "newpipe/auth_store.hpp"
#include "newpipe/ui_text.hpp"
#include "tab/library_tab.hpp"

#include "activity/stream_detail_activity.hpp"
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
bool cached_cloud=false,cached_favorites=false,cached_history=false;
}

LibraryTab::LibraryTab() : service_() {
  this->inflateFromXMLRes("xml/tabs/library.xml");
  brls::delay(700, [this, life = life_]() {
    if (life->alive)
      interactionReady_.store(true);
  });

  // Kept on the content only: clearing is destructive and needs no sidebar
  // shortcut, an empty section has nothing to clear anyway.
  this->registerAction(newpipe::tr("library/clear_action"),
                       brls::ControllerButton::BUTTON_RB, [this](brls::View *) {
                         this->clearCurrentSection();
                         return true;
                       });

  this->refresh();
}

void LibraryTab::loadAccountHistory() {
  if(!newpipe::AuthStore::instance().session().has_cookies()){brls::Application::notify(newpipe::ui("Importa prima i cookie YouTube nelle impostazioni account"));return;}
  cloud_=true;history_tv_=true;if(spinner)spinner->setVisibility(brls::Visibility::VISIBLE);
  newpipe::ui_job(life_,[]{newpipe::YouTubeCatalogService service;auto feed=service.experimental_account_feed(true);return std::make_pair(feed,service.error_message());},[this](auto result,const std::string& error){
    items_.clear();if(result.first)items_=result.first->items;buildGrid();
    if(spinner)spinner->setVisibility(brls::Visibility::GONE);
    if(statusLabel)statusLabel->setText(result.first?result.first->kiosk.title:newpipe::ui("Cronologia YouTube"));
    if(bodyLabel)bodyLabel->setText(!error.empty()?error:result.second);
  });
}
void LibraryTab::onCreate() {
  this->registerTabAction(newpipe::ui("Raccolta"), brls::ControllerButton::BUTTON_Y,
                          [this](brls::View *) {
                            std::vector<std::string> options={newpipe::ui("Cronologia YouTube"),newpipe::ui("Cronologia KaiTube (locale)"),newpipe::ui("Salvati in KaiTube (locali)")};
                            auto* menu=new brls::Dropdown(newpipe::tr("app/library"),options,[this,life=life_](int choice){
                              if(!life->alive)return;
                              if(choice==0)loadAccountHistory();
                              else if(choice==1 || choice==2){cloud_=false;showingFavorites_=choice==2;refresh();}
                            });brls::Application::pushActivity(new brls::Activity(menu));
                            return true;
                          });
  // Mirrored on the sidebar item: an empty history/favorites list has no
  // focusable child, so a content-only action could never be triggered.
  this->registerTabAction(newpipe::tr("common/refresh"),
                          brls::ControllerButton::BUTTON_X,
                          [this](brls::View *) {
                            this->refresh();
                            return true;
                          });
  this->registerTabAction(newpipe::tr("library/section_action"),
                          brls::ControllerButton::BUTTON_LB,
                          [this](brls::View *) {
                            this->toggleSection();
                            return true;
                          });
}

bool LibraryTab::allowInitialInput() const { return interactionReady_.load(); }

void LibraryTab::refresh() {
  if(newpipe::restore_video_focus && !newpipe::restore_activity && newpipe::ui_tab==3 && !cached_items.empty()) {
    items_=cached_items;history_tv_=cached_history;cloud_=cached_cloud;showingFavorites_=cached_favorites;interactionReady_=true;buildGrid();
    if(spinner)spinner->setVisibility(brls::Visibility::GONE);return;
  }
  if (cloud_) {
    loadAccountHistory();
    return;
  }
  ++life_->generation;
  if (this->spinner) {
    this->spinner->setVisibility(brls::Visibility::VISIBLE);
  }

  std::string error;
  newpipe::LibraryStore::instance().load(&error);
  this->items_ = this->showingFavorites_
                     ? newpipe::LibraryStore::instance().favorite_items()
                     : newpipe::LibraryStore::instance().history_items();
  this->buildGrid();

  if (this->statusLabel) {
    this->statusLabel->setText(newpipe::tr(
        "common/count_with_title",
        this->showingFavorites_ ? std::string(newpipe::ui("Salvati in KaiTube (locali)"))
                                : std::string(newpipe::ui("Cronologia KaiTube (locale)")),
        this->items_.size()));
  }
  if (this->bodyLabel) {
    if (this->items_.empty()) {
      this->bodyLabel->setText(this->showingFavorites_
                                   ? newpipe::tr("library/favorites_empty")
                                   : newpipe::tr("library/history_empty"));
    } else {
      this->bodyLabel->setText(newpipe::tr("library/controls"));
    }
  }
  if (this->spinner) {
    this->spinner->setVisibility(brls::Visibility::GONE);
  }
}

void LibraryTab::buildGrid() {
  cached_items=items_;cached_history=history_tv_;cached_cloud=cloud_;cached_favorites=showingFavorites_;
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
  if(newpipe::restore_video_focus && !newpipe::restore_activity && newpipe::ui_tab==3) {
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

void LibraryTab::toggleSection() {
  cloud_ = false;
  this->showingFavorites_ = !this->showingFavorites_;
  this->refresh();
}

void LibraryTab::clearCurrentSection() {
  if (cloud_) {
    brls::Application::notify(newpipe::ui("La cronologia YouTube è in sola lettura"));
    return;
  }
  std::string error;
  const bool ok =
      this->showingFavorites_
          ? newpipe::LibraryStore::instance().clear_favorites(&error)
          : newpipe::LibraryStore::instance().clear_history(&error);
  if (!ok) {
    brls::Application::notify(
        error.empty() ? newpipe::tr("library/clear_failed") : error);
    return;
  }

  brls::Application::notify(this->showingFavorites_
                                ? newpipe::tr("library/favorites_cleared")
                                : newpipe::tr("library/history_cleared"));
  this->refresh();
}

void LibraryTab::playStream(const newpipe::StreamItem &item) {
  if (!allowInitialInput()) {
    return;
  }
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

void LibraryTab::openStream(const newpipe::StreamItem &item) {
  if (!allowInitialInput()) {
    return;
  }
  brls::Application::pushActivity(new StreamDetailActivity(item));
}
