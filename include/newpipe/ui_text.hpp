// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "nlohmann/json.hpp"
#include <memory>
#include <fstream>
#include <string>
namespace newpipe {
inline std::shared_ptr<const nlohmann::json> ui_dictionary=std::make_shared<const nlohmann::json>(nlohmann::json::object());
inline std::string ui_locale="en-US";
inline void load_ui_text(std::string locale) {
 ui_locale=locale;
#ifdef __SWITCH__
 const std::string root="romfs:/i18n/";
#else
 const std::string root="resources/i18n/";
#endif
 auto result=std::make_shared<nlohmann::json>(nlohmann::json::object());
 for(const auto& code:{std::string("en-US"),locale}) {
  std::ifstream f(root+code+"/extra.json");
  if(f){auto j=nlohmann::json::parse(f,nullptr,false);if(j.is_object())result->update(j);}
 }
 std::atomic_store(&ui_dictionary,std::shared_ptr<const nlohmann::json>(result));
}
inline std::string ui(const std::string& source) {
 auto dict=std::atomic_load(&ui_dictionary);auto it=dict->find(source);
 return it!=dict->end() && it->is_string()?it->get<std::string>():source;
}
}
