#pragma once
#include "nlohmann/json.hpp"
#include <algorithm>
#include <fstream>
#include <string>
#include <vector>
namespace newpipe {
inline std::string search_history_path() {
#ifdef __SWITCH__
  return "sdmc:/switch/KaiTube-Native/search-history.json";
#else
  return "search-history.json";
#endif
}
inline std::vector<std::string> recent_searches() {
  std::ifstream f(search_history_path());
  auto j = nlohmann::json::parse(f, nullptr, false);
  std::vector<std::string> v;
  if (j.is_array())
    for (auto &s : j)
      if (s.is_string() && v.size() < 30)
        v.push_back(s.get<std::string>());
  return v;
}
inline void save_search(const std::string &query) {
  if (query.empty())
    return;
  auto v = recent_searches();
  v.erase(std::remove(v.begin(), v.end(), query), v.end());
  v.insert(v.begin(), query);
  if (v.size() > 30)
    v.resize(30);
  std::ofstream f(search_history_path());
  f << nlohmann::json(v).dump(2);
}
} // namespace newpipe
