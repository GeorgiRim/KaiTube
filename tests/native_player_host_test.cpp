#include "newpipe/log.hpp"
#include "newpipe/native_player.hpp"
#include <SDL2/SDL.h>
#include <cstdio>
#include <cstdlib>
#include <libavformat/avio.h>
#include <string>
int64_t read_cache(void *cookie, char *data, uint64_t size) {
  return fread(data, 1, size, static_cast<FILE *>(cookie));
}
int64_t seek_cache(void *cookie, int64_t off, int whence) {
  auto *f = static_cast<FILE *>(cookie);
  if (whence == AVSEEK_SIZE) {
    auto pos = ftello(f);
    fseeko(f, 0, SEEK_END);
    auto size = ftello(f);
    fseeko(f, pos, SEEK_SET);
    return size;
  }
  if (fseeko(f, off, whence & ~AVSEEK_FORCE))
    return -1;
  return ftello(f);
}
int main(int argc, char **argv) {
  if (argc < 2)
    return 2;
  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0)
    return 3;
  SDL_Window *window = SDL_CreateWindow("test", 0, 0, 640, 360, 0);
  if (!window)
    return 4;
  bool url = std::string(argv[1]).rfind("url:", 0) == 0;
  FILE *file = url ? nullptr : fopen(argv[1], "rb");
  if (!file && !url)
    return 5;
  newpipe::init_log();
  newpipe::NativePlaybackState state;
  if (argc > 2)
    state.speed = atof(argv[2]);
  if (argc > 3)
    state.start = atof(argv[3]);
  newpipe::NativeMediaInput input;
  if (url)
    input.url = std::string(argv[1]).substr(4);
  else {
    input.read = read_cache;
    input.seek = seek_cache;
    input.cookie = file;
  }
  input.state = &state;
  FILE *extra = nullptr;
  if (argc > 4) {
    extra = fopen(argv[4], "rb");
    input.audio = std::make_shared<newpipe::NativeMediaInput>();
    input.audio->read = read_cache;
    input.audio->seek = seek_cache;
    input.audio->cookie = extra;
  }
  std::string error;
  bool ok = newpipe::run_native_media(window, input, error);
  printf("native result=%d speed=%.2f start=%.2f error=%s\n", ok, state.speed,
         state.start, error.c_str());
  newpipe::shutdown_log();
  if (file)
    fclose(file);
  if (extra)
    fclose(extra);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return ok ? 0 : 1;
}
