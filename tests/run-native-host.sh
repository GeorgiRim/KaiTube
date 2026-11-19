#!/usr/bin/env bash
# Host regression only: does not exercise the Nintendo Switch 2 services.
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/host/native-fixtures
flags=$(pkg-config --cflags --libs sdl2 libavformat libavcodec libavfilter libavutil libswscale libcurl)
${CXX:-g++} -std=c++17 -O1 -DKAITUBE_HOST_CURL -Iinclude -Ivendor/third_party -Ivendor/borealis/library/include \
    tests/native_player_host_test.cpp src/switch/native_player.cpp src/switch/native_text.cpp \
    src/switch/audio_sink.cpp src/common/log.cpp src/common/captions.cpp src/common/http_client.cpp \
    src/common/settings_store.cpp src/common/http_proxy.cpp \
    $flags -pthread -o build/host/native_player_test
fixture="$PWD/build/host/native-fixtures"
ffmpeg -y -v error -f lavfi -i testsrc2=size=320x180:rate=24 -f lavfi -i sine=frequency=440:sample_rate=48000 \
    -t 4 -c:v libx264 -preset ultrafast -pix_fmt yuv420p -c:a aac -movflags +faststart "$fixture/muxed.mp4"
ffmpeg -y -v error -i "$fixture/muxed.mp4" -an -c copy "$fixture/video.mp4"
ffmpeg -y -v error -i "$fixture/muxed.mp4" -vn -c copy "$fixture/audio.m4a"
ffmpeg -y -v error -i "$fixture/muxed.mp4" -c copy -f hls -hls_time 1 -hls_list_size 0 "$fixture/local.m3u8"
export SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy
timeout 12 build/host/native_player_test "$fixture/muxed.mp4" 1 0
timeout 12 build/host/native_player_test "$fixture/muxed.mp4" 1.5 1
timeout 12 build/host/native_player_test "$fixture/video.mp4" 0.75 0 "$fixture/audio.m4a"
timeout 12 build/host/native_player_test "url:$fixture/local.m3u8" 1 0
python3 tests/native_proxy_test.py build/host/native_player_test "$fixture"
