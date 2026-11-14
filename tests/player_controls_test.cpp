#include "newpipe/native_player.hpp"
#include <SDL2/SDL.h>
#include <thread>
#include <chrono>
#include <cassert>
#include <iostream>
#include <vector>
static void run(const char* file,const std::vector<SDL_Keycode>& keys,int scenario){
 SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO);auto*w=SDL_CreateWindow("test",0,0,640,360,0);
 newpipe::NativePlaybackState state;newpipe::NativeMediaInput input;input.url=file;input.state=&state;input.quality="360p";
 std::thread events([keys]{std::this_thread::sleep_for(std::chrono::milliseconds(200));for(auto key:keys){SDL_Event e{};e.type=SDL_KEYDOWN;e.key.keysym.sym=key;SDL_PushEvent(&e);std::this_thread::sleep_for(std::chrono::milliseconds(120));}});
 std::string error;bool ok=newpipe::run_native_media(w,input,error);events.join();assert(ok);
 if(scenario==0)assert(state.restart_at<0&&state.requested_height<0);
 if(scenario==1)assert(state.seek_count==1&&state.last_seek>3.8&&state.requested_height<0);
 if(scenario==2)assert(state.restart_at<0);
 SDL_DestroyWindow(w);SDL_Quit();
}
int main(int argc,char**argv){assert(argc==2);
 run(argv[1],{SDLK_x,SDLK_SPACE,SDLK_ESCAPE},0);
 run(argv[1],{SDLK_y,SDLK_RIGHT,SDLK_SPACE},1);
 run(argv[1],{SDLK_y,SDLK_RIGHT,SDLK_ESCAPE,SDLK_ESCAPE},2);
 std::cout<<"same quality no restart; timeline confirm/cancel: passed\n";
}
