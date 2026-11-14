#pragma once

#include <atomic>
#include <vector>

#include <borealis.hpp>
#include "newpipe/ui_job.hpp"

#include "newpipe/models.hpp"
#include "newpipe/youtube_catalog_service.hpp"
#include "view/auto_tab_frame.hpp"

class LibraryTab : public AttachedView {
public:
    LibraryTab();
    ~LibraryTab() override {life_->alive=false;}

    void onCreate() override;

    static brls::View* create() { return new LibraryTab(); }

private:
    void refresh();
    void loadAccountHistory();
    void buildGrid();
    void toggleSection();
    void clearCurrentSection();
    bool allowInitialInput() const;
    void playStream(const newpipe::StreamItem& item);
    void openStream(const newpipe::StreamItem& item);

    BRLS_BIND(brls::Label, statusLabel, "library/status");
    BRLS_BIND(brls::Label, bodyLabel, "library/body");
    BRLS_BIND(brls::ProgressSpinner, spinner, "library/spinner");
    BRLS_BIND(brls::ScrollingFrame, scrollFrame, "library/scroll");
    BRLS_BIND(brls::Box, gridBox, "library/grid");

    newpipe::YouTubeCatalogService service_;
    std::vector<newpipe::StreamItem> items_;
    std::shared_ptr<newpipe::UiLifetime> life_=std::make_shared<newpipe::UiLifetime>();
    bool cloud_=false,history_tv_=false;
    bool showingFavorites_ = false;
    std::atomic<bool> interactionReady_{false};
};
