#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
namespace newpipe {
class AudioSink {
  struct Impl;
  std::unique_ptr<Impl> impl_;

public:
  AudioSink();
  ~AudioSink();
  bool reset(std::string &error);
  bool open(std::string &error);
  size_t enqueue(const int16_t *stereo, size_t frames);
  bool update(std::string &error);
  uint64_t played() const;
  uint64_t queued() const;
  void pause(bool paused);
  void volume(float value);
};
} // namespace newpipe
