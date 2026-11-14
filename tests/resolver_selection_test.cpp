// Exercise the production HLS selector, including audio lines after variants.
#include "../src/common/youtube_resolver.cpp"
#include <cassert>
#include <iostream>
int main() {
 const std::string manifest=R"(#EXTM3U
#EXT-X-STREAM-INF:BANDWIDTH=9000000,RESOLUTION=3840x2160,CODECS="hvc1.1",AUDIO="a"
4k.m3u8
#EXT-X-STREAM-INF:BANDWIDTH=2000000,RESOLUTION=1280x720,CODECS="avc1.64001f",AUDIO="a"
720.m3u8
#EXT-X-STREAM-INF:BANDWIDTH=1000000,RESOLUTION=640x360,CODECS="avc1.64001e",AUDIO="a"
360.m3u8
#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID="a",NAME="English",LANGUAGE="en",URI="dub.m3u8"
#EXT-X-MEDIA:TYPE=AUDIO,GROUP-ID="a",NAME="Italian original",LANGUAGE="it",URI="original.m3u8"
)";
 auto result=newpipe::pick_preferred_hls_playback("https://example.com/master.m3u8",manifest,720);
 assert(result&&result->selected_height==720&&result->video_url=="https://example.com/720.m3u8");
 assert(result->audio_url=="https://example.com/original.m3u8");
 result=newpipe::pick_preferred_hls_playback("https://example.com/master.m3u8",manifest,1080);
 assert(result&&result->selected_height==720);
 std::cout<<"HLS resolution/codec/audio selection tests passed\n";
}
