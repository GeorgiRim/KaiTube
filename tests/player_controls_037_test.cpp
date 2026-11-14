#include "newpipe/native_player.hpp"
#include <SDL2/SDL.h>
#include <thread>
#include <chrono>
#include <cassert>
#include <iostream>
#include <vector>
static void scenario(const char* file,std::vector<SDL_Keycode> keys,double lo,double hi,bool analog=false){
 assert(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO)==0);auto*w=SDL_CreateWindow("controls",0,0,640,360,0);assert(w);
 newpipe::NativePlaybackState state;newpipe::NativeMediaInput source;source.url=file;source.state=&state;source.quality="360p";
 std::thread events([=]{std::this_thread::sleep_for(std::chrono::milliseconds(250));
  if(analog){SDL_Event a{};a.type=SDL_CONTROLLERAXISMOTION;a.caxis.axis=SDL_CONTROLLER_AXIS_RIGHTX;a.caxis.value=20000;SDL_PushEvent(&a);std::this_thread::sleep_for(std::chrono::milliseconds(150));}
  for(auto k:keys){SDL_Event e{};e.type=SDL_KEYDOWN;e.key.keysym.sym=k;SDL_PushEvent(&e);std::this_thread::sleep_for(std::chrono::milliseconds(150));}
  std::this_thread::sleep_for(std::chrono::milliseconds(400));SDL_Event stop{};stop.type=SDL_QUIT;SDL_PushEvent(&stop);
 });
 std::string error;bool ok=newpipe::run_native_media(w,source,error);events.join();assert(ok && error.empty());
 if(hi>=0)assert(state.seek_count==1 && state.last_seek>=lo && state.last_seek<=hi);else assert(state.seek_count==0 && state.restart_at<0);SDL_DestroyWindow(w);SDL_Quit();
}
int main(int argc,char**argv){assert(argc==2);
 scenario(argv[1],{SDLK_e,SDLK_SPACE},29,32);
 scenario(argv[1],{SDLK_e,SDLK_q,SDLK_l,SDLK_SPACE},9,12);
 scenario(argv[1],{SDLK_RIGHT,SDLK_SPACE},9,12);
 scenario(argv[1],{SDLK_SPACE},9,12,true);
 scenario(argv[1],{SDLK_e,SDLK_ESCAPE,SDLK_ESCAPE},-1,-1);
 std::cout<<"30/10 second preview, direction, analog input and cancel passed\n";
}
