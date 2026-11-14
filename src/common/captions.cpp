// SPDX-License-Identifier: GPL-3.0-or-later
#include "newpipe/captions.hpp"
#include "nlohmann/json.hpp"
#include <algorithm>
#include <regex>
#include <sstream>
namespace newpipe {
std::string caption_url_with_format(const std::string &url,
                                    const std::string &format) {
  auto q = url.find('?');
  std::string out = url.substr(0, q);
  std::vector<std::string> fields;
  if (q != std::string::npos) {
    std::istringstream query(url.substr(q + 1));
    std::string field;
    while (std::getline(query, field, '&'))
      if (!field.empty() && field.rfind("fmt=", 0) != 0)
        fields.push_back(field);
  }
  fields.push_back("fmt=" + format);
  for (size_t i = 0; i < fields.size(); ++i)
    out += (i ? "&" : "?") + fields[i];
  return out;
}
std::vector<CaptionCue> parse_captions(const std::string &text) {
  std::vector<CaptionCue> out;
  auto root = nlohmann::json::parse(text, nullptr, false);
  if (!root.is_discarded() && root.contains("events") &&
      root["events"].is_array()) {
    for (const auto &e : root["events"]) {
      if (!e.contains("segs") || !e["segs"].is_array())
        continue;
      CaptionCue cue;
      cue.start = e.value("tStartMs", 0.0) / 1000.0;
      cue.end = cue.start + e.value("dDurationMs", 3000.0) / 1000.0;
      for (const auto &s : e["segs"])
        cue.text += s.value("utf8", "");
      if (!cue.text.empty() && cue.end > cue.start)
        out.push_back(std::move(cue));
    }
  } else {
    static const std::regex timing(
        R"((?:(\d+):)?(\d\d):(\d\d)[.,](\d\d\d)\s+-->\s+(?:(\d+):)?(\d\d):(\d\d)[.,](\d\d\d))");
    std::istringstream stream(text);
    std::string line;
    CaptionCue cue;
    bool reading = false;
    auto push = [&] {
      if (reading && !cue.text.empty() && cue.end > cue.start)
        out.push_back(cue);
      reading = false;
    };
    while (std::getline(stream, line)) {
      if (!line.empty() && line.back() == '\r')
        line.pop_back();
      std::smatch m;
      if (std::regex_search(line, m, timing)) {
        push();
        cue = {};
        auto time = [&](int i) {
          return (m[i].matched ? std::stod(m[i]) * 3600 : 0) +
                 std::stod(m[i + 1]) * 60 + std::stod(m[i + 2]) +
                 std::stod(m[i + 3]) / 1000;
        };
        cue.start = time(1);
        cue.end = time(5);
        reading = true;
      } else if (line.empty())
        push();
      else if (reading) {
        line = std::regex_replace(line, std::regex("<[^>]*>"), "");
        cue.text += (cue.text.empty() ? "" : "\n") + line;
      }
    }
    push();
  }
  std::stable_sort(out.begin(), out.end(),
                   [](const CaptionCue &a, const CaptionCue &b) {
                     return a.start < b.start;
                   });
  return out;
}
std::string caption_at(const std::vector<CaptionCue> &cues, double time) {
  auto it = std::upper_bound(
      cues.begin(), cues.end(), time,
      [](double t, const CaptionCue &c) { return t < c.start; });
  if (it == cues.begin())
    return {};
  --it;
  return time < it->end ? it->text : std::string{};
}
} // namespace newpipe
