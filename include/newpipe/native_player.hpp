#pragma once
#include "newpipe/captions.hpp"
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
struct SDL_Window;
namespace newpipe {
struct NativePlaybackState {
  double start = 0, speed = 1, restart_at = -1;
  unsigned seek_count=0;
  double last_seek=-1;
  int requested_height = -1;
  int caption_index = -1;
  bool software_only = false, resume_fallback = false;
};
struct NativeMediaInput {
  int64_t (*read)(void *, char *, uint64_t) = nullptr;
  int64_t (*seek)(void *, int64_t, int) = nullptr;
  void *cookie = nullptr;
  std::string resume_key, thumbnail_jpeg;
  std::string url, user_agent, referer, title, quality;
  std::shared_ptr<std::atomic_bool> cancelled =
      std::make_shared<std::atomic_bool>(false);
  std::shared_ptr<NativeMediaInput> audio;
  std::function<double()> buffered_ratio;
  std::function<bool(double,double)> prepare_seek;
  std::vector<CaptionTrack> captions;
  NativePlaybackState *state = nullptr;
};
bool run_native_media(SDL_Window *window, const NativeMediaInput &input,
                      std::string &error);
} // namespace newpipe
