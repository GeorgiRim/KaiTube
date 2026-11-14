#include "newpipe/native_player.hpp"
#include <SDL2/SDL.h>
#include <thread>
#include <chrono>
#include <cassert>
#include <iostream>
int main(int argc,char**argv){assert(argc==2);SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO);auto*w=SDL_CreateWindow("tabs",0,0,640,360,0);
 newpipe::NativePlaybackState state;newpipe::NativeMediaInput input;input.url=argv[1];input.state=&state;input.quality="360p";
 std::thread events([]{std::this_thread::sleep_for(std::chrono::milliseconds(250));for(auto k:{SDLK_x,SDLK_RIGHT,SDLK_SPACE,SDLK_RIGHT,SDLK_SPACE}){SDL_Event e{};e.type=SDL_KEYDOWN;e.key.keysym.sym=k;SDL_PushEvent(&e);std::this_thread::sleep_for(std::chrono::milliseconds(140));}});
 std::string error;assert(newpipe::run_native_media(w,input,error));events.join();assert(error.empty());assert(state.speed==1.25 && state.requested_height<0 && state.restart_at>=0);
 SDL_DestroyWindow(w);SDL_Quit();std::cout<<"Horizontal tab navigation and value confirmation passed\n";
}
