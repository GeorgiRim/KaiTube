// SPDX-License-Identifier: GPL-3.0-or-later
#include "newpipe/native_text.hpp"
#include "newpipe/ui_text.hpp"
#ifdef __SWITCH__
#include <switch.h>
#endif
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#include <borealis/extern/nanovg/stb_image.h>
#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#include <borealis/extern/nanovg/stb_truetype.h>
#pragma GCC diagnostic pop
#include <algorithm>
#include <fstream>
#include <iterator>
#include <unordered_map>
#include <vector>
namespace newpipe {
static std::vector<int> points(const std::string &s) {
  std::vector<int> out;
  for (size_t i = 0; i < s.size() && out.size() < 240;) {
    unsigned char c = s[i++];
    int cp = c, count = 0;
    if (c >= 0xf0) {
      cp = c & 7;
      count = 3;
    } else if (c >= 0xe0) {
      cp = c & 15;
      count = 2;
    } else if (c >= 0xc0) {
      cp = c & 31;
      count = 1;
    }
    for (int k = 0; k < count; ++k) {
      if (i >= s.size() || (static_cast<unsigned char>(s[i]) & 0xc0) != 0x80) {
        cp = 0xfffd;
        break;
      }
      cp = (cp << 6) | (s[i++] & 63);
    }
    out.push_back(cp);
  }
  return out;
}
struct NativeText::Impl {
  SDL_Renderer *renderer;
  stbtt_fontinfo font{};
  std::vector<unsigned char> data;
  struct Entry {
    SDL_Texture *texture;
    int w, h;
  };
  std::unordered_map<std::string, Entry> cache;
  ~Impl() {
    for (auto &e : cache)
      SDL_DestroyTexture(e.second.texture);
  }
};
NativeText::NativeText(SDL_Renderer *renderer) : impl_(new Impl{renderer}) {}
NativeText::~NativeText() = default;
bool NativeText::load(const std::string &path) {
#ifdef __SWITCH__
  if(ui_locale=="ko" && R_SUCCEEDED(plInitialize(PlServiceType_User))) {
    PlFontData shared{};
    if(R_SUCCEEDED(plGetSharedFontByType(&shared,PlSharedFontType_KO))) {
      const auto* begin=static_cast<const unsigned char*>(shared.address);
      impl_->data.assign(begin,begin+shared.size);
    }
    plExit();
    if(!impl_->data.empty() && stbtt_InitFont(&impl_->font,impl_->data.data(),0))return true;
    impl_->data.clear();
  }
#endif
  std::ifstream f(path, std::ios::binary);
  if (!f)
    return false;
  impl_->data.assign(std::istreambuf_iterator<char>(f), {});
  return !impl_->data.empty() &&
         stbtt_InitFont(&impl_->font, impl_->data.data(), 0);
}
SDL_Texture* NativeText::thumbnail(const std::string& jpeg) {
 if(jpeg.empty() || jpeg.size()>2*1024*1024)return nullptr;
 int w,h,n;auto* pixels=stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(jpeg.data()),jpeg.size(),&w,&h,&n,4);
 if(!pixels)return nullptr;
 SDL_Texture* texture=nullptr;
 auto* surface=SDL_CreateRGBSurfaceWithFormatFrom(pixels,w,h,32,w*4,SDL_PIXELFORMAT_RGBA32);
 if(surface){texture=SDL_CreateTextureFromSurface(impl_->renderer,surface);SDL_FreeSurface(surface);}
 stbi_image_free(pixels);return texture;
}
int NativeText::width(const std::string &text, int pixels) {
  if (impl_->data.empty())
    return 0;
  float scale = stbtt_ScaleForPixelHeight(&impl_->font, pixels);
  float x = 0;
  int previous = 0;
  for (int cp : points(text)) {
    int advance, bearing;
    stbtt_GetCodepointHMetrics(&impl_->font, cp, &advance, &bearing);
    x += (advance + stbtt_GetCodepointKernAdvance(&impl_->font, previous, cp)) *
         scale;
    previous = cp;
  }
  return static_cast<int>(x + 0.5f);
}
void NativeText::draw(const std::string &text, int x, int y, int pixels,
                      SDL_Color color, int max_width) {
  auto &p = *impl_;
  if (text.empty() || p.data.empty())
    return;
  std::string key = std::to_string(pixels) + ':' + text;
  auto it = p.cache.find(key);
  if (it == p.cache.end()) {
    if (p.cache.size() >= 48) {
      SDL_DestroyTexture(p.cache.begin()->second.texture);
      p.cache.erase(p.cache.begin());
    }
    const int w = std::clamp(width(text, pixels) + 8, 8, 3072), h = pixels + 12;
    std::vector<unsigned char> rgba(static_cast<size_t>(w) * h * 4, 0);
    float scale = stbtt_ScaleForPixelHeight(&p.font, pixels), cursor = 4;
    int ascent, descent, gap;
    stbtt_GetFontVMetrics(&p.font, &ascent, &descent, &gap);
    int baseline = static_cast<int>(ascent * scale) + 4, previous = 0;
    for (int cp : points(text)) {
      cursor += stbtt_GetCodepointKernAdvance(&p.font, previous, cp) * scale;
      int bw, bh, ox, oy;
      auto *bitmap = stbtt_GetCodepointBitmap(&p.font, scale, scale, cp, &bw,
                                              &bh, &ox, &oy);
      for (int by = 0; by < bh; ++by)
        for (int bx = 0; bx < bw; ++bx) {
          int dx = static_cast<int>(cursor) + ox + bx, dy = baseline + oy + by;
          if (dx < 0 || dx >= w || dy < 0 || dy >= h)
            continue;
          auto *dest = &rgba[(static_cast<size_t>(dy) * w + dx) * 4];
          dest[0] = dest[1] = dest[2] = 255;
          dest[3] = bitmap[by * bw + bx];
        }
      stbtt_FreeBitmap(bitmap, nullptr);
      int advance, bearing;
      stbtt_GetCodepointHMetrics(&p.font, cp, &advance, &bearing);
      cursor += advance * scale;
      previous = cp;
    }
    SDL_Texture *texture = SDL_CreateTexture(p.renderer, SDL_PIXELFORMAT_RGBA32,
                                             SDL_TEXTUREACCESS_STATIC, w, h);
    if (!texture)
      return;
    SDL_UpdateTexture(texture, nullptr, rgba.data(), w * 4);
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    it = p.cache.emplace(key, Impl::Entry{texture, w, h}).first;
  }
  const auto &e = it->second;
  SDL_SetTextureColorMod(e.texture, color.r, color.g, color.b);
  SDL_SetTextureAlphaMod(e.texture, color.a);
  int w = max_width > 0 ? std::min(e.w, max_width) : e.w;
  SDL_Rect source{0, 0, w, e.h}, target{x, y, w, e.h};
  SDL_RenderCopy(p.renderer, e.texture, &source, &target);
}
} // namespace newpipe
