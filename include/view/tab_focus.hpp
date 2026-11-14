#pragma once

#include <borealis.hpp>

#include "view/auto_tab_frame.hpp"

namespace newpipe {
inline std::string focused_video;
inline bool restore_video_focus=false;

// Call this before clearing a card grid.
//
// borealis clears Application::currentFocus when the focused view is destroyed
// (View::~View focus sanity check), and Application::navigate() bails out while
// there is no focus, so deleting the focused card leaves the whole UI unable to
// move. Handing focus to the scrolling frame does not help: its default focus
// resolves through lastFocusedView back to the very card that is about to be
// deleted, which makes giveFocus() a no-op.
//
// The sidebar item is always focusable and never owned by the grid, so it is a
// safe place to park focus. Giving it focus does not switch tabs: the item is
// already the active one, and AutoSidebarItem::setActive() ignores no-op
// changes.
inline void release_grid_focus(AttachedView* tab, brls::Box* gridBox) {
    if (!gridBox) {
        return;
    }

    for (brls::View* view = brls::Application::getCurrentFocus(); view; view = view->getParent()) {
        if (view != gridBox) {
            continue;
        }

        brls::View* sidebarItem = tab ? tab->getTabBar() : nullptr;
        if (sidebarItem) {
            brls::Application::giveFocus(sidebarItem);
        }
        return;
    }
}

inline void wire_grid_navigation(brls::Box* grid) {
    const auto& rows = grid->getChildren();
    for (size_t r=0;r<rows.size();++r) {
        auto* row=dynamic_cast<brls::Box*>(rows[r]);
        if (!row) continue;
        const auto& cards=row->getChildren();
        for(size_t c=0;c<cards.size();++c) {
            if(r>0) {
                auto* prev=dynamic_cast<brls::Box*>(rows[r-1]);
                if(prev && !prev->getChildren().empty())
                    cards[c]->setCustomNavigationRoute(brls::FocusDirection::UP,
                      prev->getChildren()[std::min(c,prev->getChildren().size()-1)]);
            }
            if(r+1<rows.size()) {
                auto* next=dynamic_cast<brls::Box*>(rows[r+1]);
                if(next && !next->getChildren().empty())
                    cards[c]->setCustomNavigationRoute(brls::FocusDirection::DOWN,
                      next->getChildren()[std::min(c,next->getChildren().size()-1)]);
            }
        }
    }
}
}  // namespace newpipe
