#include "newpipe/ui_text.hpp"
#include "tab/search_tab.hpp"
#include "newpipe/search_history.hpp"

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
std::string cached_query;
}

SearchTab::SearchTab() : service_() {
  this->inflateFromXMLRes("xml/tabs/search.xml");
  newpipe::log_line("search: construct");
  if (statusLabel) {
    statusLabel->setText(newpipe::tr("search/prompt"));
  }
  brls::delay(700, [this, life = life_]() {
    if (!life->alive)
      return;
    interactionReady_.store(true);
    newpipe::log_line("search: interaction ready");
  });
}

void SearchTab::onCreate() {
 if(newpipe::restore_video_focus && !newpipe::restore_activity && newpipe::ui_tab==1 && !cached_items.empty()) {
   items_=cached_items;lastQuery_=cached_query;interactionReady_=true;buildGrid();
   if(statusLabel)statusLabel->setText(newpipe::tr("search/results_count",lastQuery_,items_.size()));
 }

  this->registerTabAction(
      newpipe::ui("Ricerche recenti"), brls::ControllerButton::BUTTON_Y,
      [this](brls::View *) {
        auto history = newpipe::recent_searches();
        auto labels = history;
        labels.push_back(newpipe::ui("Cancella cronologia ricerche"));
        auto *dropdown = new brls::Dropdown(
            newpipe::ui("Ricerche recenti (solo KaiTube)"), labels,
            [this, life = life_, history](int selected) {
              if (!life->alive || selected < 0)
                return;
              if (selected == static_cast<int>(history.size()))
                std::remove(newpipe::search_history_path().c_str());
              else if (selected < static_cast<int>(history.size()))
                doSearch(history[selected]);
            });
        brls::Application::pushActivity(new brls::Activity(dropdown));
        return true;
      });
  // Registered on the sidebar item as well: until a query returns results this
  // tab has no focusable child, so focus can never leave the sidebar and a
  // content-only action would be unreachable.
  this->registerTabAction(
      newpipe::tr("search/action"), brls::ControllerButton::BUTTON_X,
      [this](brls::View *) {
        brls::Application::getImeManager()->openForText(
            [this](const std::string &text) { doSearch(text); },
            newpipe::tr("search/ime_title"), newpipe::tr("search/ime_subtitle"),
            80, lastQuery_);
        return true;
      });
}

void SearchTab::doSearch(const std::string &query) {
  lastQuery_ = query; cached_query=query;
  newpipe::save_search(query);
  newpipe::logf("search: doSearch query=%s", query.c_str());

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

  if (query.empty()) {
    ++life_->generation;
    items_.clear();
    if (gridBox) {
      newpipe::release_grid_focus(this, gridBox);
      gridBox->clearViews();
    }
    if (statusLabel) {
      statusLabel->setText(newpipe::tr("search/prompt"));
    }
    if (spinner) {
      spinner->setVisibility(brls::Visibility::GONE);
    }
    return;
  }

  newpipe::ui_job(
      life_,
      [query]() {
        newpipe::YouTubeCatalogService service;
        auto results = service.search(query);
        return std::make_pair(results, service.error_message());
      },
      [this, query](auto result, const std::string &exception) {
        const auto &results = result.first;
        const std::string request_error =
            exception.empty() ? result.second : exception;
        items_ = results.items;
        cached_items=items_;
        newpipe::logf("search: results=%zu", items_.size());
        buildGrid();

        if (statusLabel) {
          if (items_.empty()) {
            if (request_error.empty()) {
              statusLabel->setText(newpipe::tr("search/no_results", query));
            } else {
              statusLabel->setText(request_error);
            }
          } else {
            statusLabel->setText(
                newpipe::tr("search/results_count", query, items_.size()));
          }
        }
        if (spinner) {
          spinner->setVisibility(brls::Visibility::GONE);
        }
      });
}

void SearchTab::buildGrid() {
  if (!gridBox) {
    newpipe::log_line("search: buildGrid gridBox missing");
    return;
  }

  newpipe::logf("search: buildGrid items=%zu", items_.size());
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
  if(newpipe::restore_video_focus && !newpipe::restore_activity && newpipe::ui_tab==1) {
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

bool SearchTab::allowInitialInput() const {
  if (interactionReady_.load()) {
    return true;
  }

  newpipe::log_line("search: ignored startup input");
  return false;
}

void SearchTab::playStream(const newpipe::StreamItem &item) {
  if (!allowInitialInput()) {
    return;
  }
  newpipe::logf("search: playStream url=%s", item.url.c_str());
  const std::optional<newpipe::StreamDetail> detail;
  const auto request = newpipe::build_playback_request(item, detail);
  if (!request.has_value()) {
    openStream(item);
    return;
  }

  std::string ignored_error;
  newpipe::LibraryStore::instance().add_history(
      detail.has_value() ? detail->item : item, &ignored_error);
  newpipe::logf("search: queue playback url=%s", request->url.c_str());
  newpipe::restore_activity={};
  newpipe::request_playback(*request);
}

void SearchTab::openStream(const newpipe::StreamItem &item) {
  if (!allowInitialInput()) {
    return;
  }
  newpipe::logf("search: openStream url=%s", item.url.c_str());
  brls::Application::pushActivity(new StreamDetailActivity(item));
}
