#include "newpipe/captions.hpp"
#include "nlohmann/json.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <unistd.h>
int main() {
  using namespace newpipe;
  assert(caption_url_with_format(
             "https://example.com/api?v=1&fmt=srv3&lang=it&fmt=xml", "json3") ==
         "https://example.com/api?v=1&lang=it&fmt=json3");
  assert(caption_url_with_format("https://example.com/api", "vtt") ==
         "https://example.com/api?fmt=vtt");
  auto cues = parse_captions(
      R"({"events":[{"tStartMs":2000,"dDurationMs":1000,"segs":[{"utf8":"due"}]},{"tStartMs":500,"dDurationMs":1000,"segs":[{"utf8":"ciao "},{"utf8":"mondo"}]}]})");
  assert(cues.size() == 2 && caption_at(cues, .49).empty());
  assert(caption_at(cues, .5) == "ciao mondo");
  assert(caption_at(cues, 1.5).empty());
  assert(caption_at(cues, 2.2) == "due");
  cues = parse_captions(
      "WEBVTT\n\n00:01.200 --> 00:02.300\n<b>ciao</b>\nmondo\n\n00:00:03,000 "
      "--> 00:00:04,000\naltro\n");
  assert(cues.size() == 2 && caption_at(cues, 1.3) == "ciao\nmondo" &&
         caption_at(cues, 3.5) == "altro");
  std::cout << "Caption parsing and format selection passed\n";
}
