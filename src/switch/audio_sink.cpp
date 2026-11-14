// SPDX-License-Identifier: GPL-3.0-or-later
#include "newpipe/audio_sink.hpp"
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <vector>
#ifdef __SWITCH__
#include <switch.h>
#else
#include <SDL2/SDL.h>
#endif
namespace newpipe {
struct AudioSink::Impl {
  uint64_t submitted = 0, consumed = 0;
  float volume = 1;
  std::string failure;
#ifdef __SWITCH__
  AudioDriver driver{};
  bool service = false, created = false;
  void *memory = nullptr;
  AudioDriverWaveBuf buffers[8]{};
  uint32_t previous_count = 0;
  static constexpr size_t samples_per_buffer = 2048;
  ~Impl() {
    if (created) {
      audrvVoiceStop(&driver, 0);
      audrvUpdate(&driver);
      audrvClose(&driver);
    }
    if (service)
      audrenExit();
    free(memory);
  }
#else
  SDL_AudioDeviceID device = 0;
  ~Impl() {
    if (device)
      SDL_CloseAudioDevice(device);
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
  }
#endif
};
AudioSink::AudioSink() : impl_(new Impl) {}
AudioSink::~AudioSink() = default;
bool AudioSink::reset(std::string &error) { impl_.reset(new Impl);return open(error); }
bool AudioSink::open(std::string &error) {
  auto &p = *impl_;
#ifdef __SWITCH__
  AudioRendererConfig config{};
  config.output_rate = AudioRendererOutputRate_48kHz;
  config.num_voices = 2;
  config.num_sinks = 1;
  config.num_mix_objs = 1;
  config.num_mix_buffers = 2;
  Result rc = audrenInitialize(&config);
  if (R_FAILED(rc)) {
    error = "audrenInitialize: " + std::to_string(rc);
    return false;
  }
  p.service = true;
  rc = audrvCreate(&p.driver, &config, 2);
  if (R_FAILED(rc)) {
    error = "audrvCreate: " + std::to_string(rc);
    return false;
  }
  p.created = true;
  constexpr size_t bytes = 8 * Impl::samples_per_buffer * 2 * sizeof(int16_t);
  p.memory = aligned_alloc(AUDREN_MEMPOOL_ALIGNMENT, bytes);
  if (!p.memory) {
    error = "Cannot allocate audio ring";
    return false;
  }
  std::memset(p.memory, 0, bytes);
  int pool = audrvMemPoolAdd(&p.driver, p.memory, bytes);
  if (pool < 0 || !audrvMemPoolAttach(&p.driver, pool)) {
    error = "Cannot attach audio ring";
    return false;
  }
  const u8 channels[] = {0, 1};
  if (audrvDeviceSinkAdd(&p.driver, AUDREN_DEFAULT_DEVICE_NAME, 2, channels) <
      0) {
    error = "Cannot create audio sink";
    return false;
  }
  if (!audrvVoiceInit(&p.driver, 0, 2, PcmFormat_Int16, 48000)) {
    error = "Cannot create audio voice";
    return false;
  }
  audrvVoiceSetDestinationMix(&p.driver, 0, AUDREN_FINAL_MIX_ID);
  audrvVoiceSetMixFactor(&p.driver, 0, 1, 0, 0);
  audrvVoiceSetMixFactor(&p.driver, 0, 1, 1, 1);
  audrvVoiceSetPaused(&p.driver, 0, true);
  for (size_t i = 0; i < 8; ++i) {
    p.buffers[i].data_raw = p.memory;
    p.buffers[i].size = bytes;
    p.buffers[i].start_sample_offset = i * Impl::samples_per_buffer;
  }
  rc = audrvUpdate(&p.driver);
  if (R_SUCCEEDED(rc))
    rc = audrenStartAudioRenderer();
  if (R_FAILED(rc)) {
    error = "Start audio renderer: " + std::to_string(rc);
    return false;
  }
#else
  if (SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
    error = SDL_GetError();
    return false;
  }
  SDL_AudioSpec wanted{};
  wanted.freq = 48000;
  wanted.format = AUDIO_S16SYS;
  wanted.channels = 2;
  wanted.samples = 1024;
  p.device = SDL_OpenAudioDevice(nullptr, 0, &wanted, nullptr, 0);
  if (!p.device) {
    error = SDL_GetError();
    return false;
  }
#endif
  return true;
}
size_t AudioSink::enqueue(const int16_t *samples, size_t frames) {
  auto &p = *impl_;
#ifdef __SWITCH__
  for (auto &b : p.buffers) {
    if (b.state != AudioDriverWaveBufState_Free &&
        b.state != AudioDriverWaveBufState_Done)
      continue;
    size_t count = std::min(frames, Impl::samples_per_buffer);
    auto *destination =
        static_cast<int16_t *>(p.memory) + b.start_sample_offset * 2;
    std::memcpy(destination, samples, count * 4);
    armDCacheFlush(destination, count * 4);
    b.end_sample_offset = b.start_sample_offset + count;
    b.next = nullptr;
    if (!audrvVoiceAddWaveBuf(&p.driver, 0, &b)) {
      p.failure = "Audio wave buffer rejected";
      return 0;
    }
    p.submitted += count;
    return count;
  }
  return 0;
#else
  if (queued() >= 16384)
    return 0;
  size_t count = std::min<size_t>(frames, 2048);
  std::vector<int16_t> mixed(count * 2);
  for (size_t i = 0; i < mixed.size(); ++i)
    mixed[i] = static_cast<int16_t>(samples[i] * p.volume);
  if (SDL_QueueAudio(p.device, mixed.data(), count * 4) < 0) {
    p.failure = SDL_GetError();
    return 0;
  }
  p.submitted += count;
  return count;
#endif
}
bool AudioSink::update(std::string &error) {
  if (!impl_->failure.empty()) {
    error = impl_->failure;
    return false;
  }
#ifdef __SWITCH__
  auto &p = *impl_;
  Result rc = audrvUpdate(&p.driver);
  if (R_FAILED(rc)) {
    error = "Audio update: " + std::to_string(rc);
    return false;
  }
  uint32_t now = audrvVoiceGetPlayedSampleCount(&p.driver, 0);
  p.consumed += static_cast<uint32_t>(now - p.previous_count);
  p.previous_count = now;
#else
  (void)error;
#endif
  return true;
}
uint64_t AudioSink::played() const {
#ifdef __SWITCH__
  return impl_->consumed;
#else
  return impl_->submitted - SDL_GetQueuedAudioSize(impl_->device) / 4;
#endif
}
uint64_t AudioSink::queued() const {
  return impl_->submitted - std::min(played(), impl_->submitted);
}
void AudioSink::pause(bool value) {
#ifdef __SWITCH__
  audrvVoiceSetPaused(&impl_->driver, 0, value);
#else
  SDL_PauseAudioDevice(impl_->device, value ? 1 : 0);
#endif
}
void AudioSink::volume(float value) {
  impl_->volume = std::clamp(value, 0.0f, 1.0f);
#ifdef __SWITCH__
  audrvVoiceSetVolume(&impl_->driver, 0, impl_->volume);
#endif
}
} // namespace newpipe
