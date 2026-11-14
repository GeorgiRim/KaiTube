// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "nlohmann/json.hpp"
#include <fstream>
#include <cstdio>
#include <cmath>
#include <sys/stat.h>
#include <string>
namespace newpipe {
inline std::string resume_path(){
#ifdef __SWITCH__
 return "sdmc:/switch/KaiTube-Native/resume.json";
#else
 return "kaitube-resume.json";
#endif
}
inline nlohmann::json resume_entries(){
 std::ifstream f(resume_path()); if(!f) return nlohmann::json::object();
 auto j=nlohmann::json::parse(f,nullptr,false); return j.is_object()?j:nlohmann::json::object();
}
inline double resume_position(const std::string& url){
 auto j=resume_entries(); if(!j.contains(url)||!j[url].is_number())return 0;
 double v=j[url].get<double>(); return std::isfinite(v)&&v>=10?v:0;
}
inline bool save_resume(const std::string& url,double pos,double duration){
 if(!std::isfinite(pos)||pos<0)return false;
 auto j=resume_entries();
 if(pos<10 || (duration>0 && pos>=duration-10))j.erase(url); else j[url]=pos;
 while(j.size()>300)j.erase(j.begin());
#ifdef __SWITCH__
 mkdir("sdmc:/switch/KaiTube-Native",0777);
#endif
 auto path=resume_path(),tmp=path+".tmp",backup=path+".bak";
 std::ofstream f(tmp);f<<j.dump();f.close();if(!f)return false;
 bool existing=static_cast<bool>(std::ifstream(path));
 if(existing){std::remove(backup.c_str());if(std::rename(path.c_str(),backup.c_str()))return false;}
 if(std::rename(tmp.c_str(),path.c_str())){if(existing)std::rename(backup.c_str(),path.c_str());return false;}
 std::remove(backup.c_str());return true;
}
}
