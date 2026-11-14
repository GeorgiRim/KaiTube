#include "view/stream_card.hpp"
#include "view/tab_focus.hpp"

#include "newpipe/image_loader.hpp"
#include "newpipe/i18n.hpp"

StreamCard::StreamCard() {
    this->inflateFromXMLRes("xml/views/stream_card.xml");
}

void StreamCard::onFocusGained() { brls::Box::onFocusGained(); newpipe::focused_video=video_url_; }

void StreamCard::setData(const newpipe::StreamItem& item) {
    video_url_=item.url;
    if (titleLabel) {
        titleLabel->setText(item.title);
    }
    if (channelLabel) {
        channelLabel->setText(item.channel_name);
    }
    if (leftMetaLabel) {
        leftMetaLabel->setText(item.is_live ? newpipe::tr("stream/live_badge") : item.duration_text);
    }
    if (rightMetaLabel) {
        rightMetaLabel->setText(item.is_live ? item.view_count_text : item.published_text);
    }
    if (thumbnail && !item.thumbnail_url.empty()) {
        newpipe::ImageLoader::instance().load(item.thumbnail_url, thumbnail);
    }
}
