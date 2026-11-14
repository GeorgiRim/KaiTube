#pragma once
#include <string>
#include <vector>
namespace newpipe {
struct CaptionTrack {
  std::string label, language, url;
  bool automatic = false;
};
struct CaptionCue {
  double start = 0, end = 0;
  std::string text;
};
std::string caption_url_with_format(const std::string& url, const std::string& format);
std::vector<CaptionCue> parse_captions(const std::string &text);
std::string caption_at(const std::vector<CaptionCue> &cues, double seconds);
} // namespace newpipe
