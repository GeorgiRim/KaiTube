#pragma once

#include <optional>
#include <functional>
#include <string>

#include "newpipe/switch_player.hpp"

namespace newpipe {

inline std::function<void()> restore_activity;
inline bool ui_reload=false;
inline int ui_tab=-1;
void queue_playback(PlaybackRequest request);
std::optional<PlaybackRequest> take_pending_playback();
void clear_pending_playback();

void set_last_playback_error(std::string error_message);
std::string take_last_playback_error();

}  // namespace newpipe
