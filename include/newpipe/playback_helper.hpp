#include "newpipe/ui_text.hpp"
#pragma once

#include <optional>
#include <borealis.hpp>
#include "view/tab_focus.hpp"
#include "newpipe/runtime.hpp"
#include "newpipe/resume_store.hpp"
#include "newpipe/image_loader.hpp"
#include <string>

#include "newpipe/models.hpp"
#include "newpipe/switch_player.hpp"

namespace newpipe {

inline void request_playback(PlaybackRequest request) {
 const double saved=resume_position(request.url);
 auto launch=[](PlaybackRequest r){restore_video_focus=true;queue_playback(std::move(r));brls::Application::quit();};
 if(saved<10){launch(request);return;}
 char time[32];std::snprintf(time,sizeof(time),"%d:%02d",(int)saved/60,(int)saved%60);
 auto* d=new brls::Dialog(std::string(newpipe::ui("Riprendere da "))+time+"?");
 d->addButton(newpipe::ui("Riprendi"),[d,request,saved,launch]() mutable {d->close();request.start_seconds=saved;launch(request);});
 d->addButton(newpipe::ui("Ricomincia"),[d,request,launch]{d->close();save_resume(request.url,0,0);launch(request);});
 d->addButton(newpipe::ui("Annulla"),[d]{d->close();});d->open();
}
inline std::optional<PlaybackRequest> build_playback_request(
    const StreamItem& item,
    const std::optional<StreamDetail>& detail) {
    std::string url = item.url;
    if (url.empty() && detail.has_value() && detail->playback_url.has_value()) {
        url = *detail->playback_url;
    }

    if (url.empty()) {
        return std::nullopt;
    }

    PlaybackRequest request;
    request.title = item.title;
    request.thumbnail_jpeg=ImageLoader::instance().cached_image(item.thumbnail_url);
    request.url = url;
    request.referer = item.url;
    if (item.url.empty()) {
        request.http_header_fields =
            "Accept: */*,Accept-Encoding: identity,Connection: close,Cache-Control: no-cache";
    }
    return request;
}

}  // namespace newpipe
