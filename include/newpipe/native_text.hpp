#pragma once
#include <SDL2/SDL.h>
#include <memory>
#include <string>
namespace newpipe {
class NativeText {
  struct Impl;
  std::unique_ptr<Impl> impl_;

public:
  explicit NativeText(SDL_Renderer *renderer);
  ~NativeText();
  bool load(const std::string &path);
  SDL_Texture* thumbnail(const std::string& jpeg);
  int width(const std::string &text, int pixels);
  void draw(const std::string &text, int x, int y, int pixels, SDL_Color color,
            int max_width = 0);
};
} // namespace newpipe
