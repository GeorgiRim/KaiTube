#include "newpipe/native_player.hpp"
#include <SDL2/SDL.h>
#include <thread>
#include <chrono>
#include <cassert>
#include <iostream>
int main(){
 using namespace std::chrono;
 assert(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO)==0);
 auto* window=SDL_CreateWindow("cancel test",0,0,640,360,0);assert(window);
 newpipe::NativeMediaInput input;
 input.cookie=input.cancelled.get();
 input.read=[](void* p,char*,uint64_t)->int64_t {
   auto* cancelled=static_cast<std::atomic_bool*>(p);
   while(!cancelled->load())std::this_thread::sleep_for(milliseconds(5));
   return -1;
 };
 std::thread events([] {std::this_thread::sleep_for(milliseconds(250));
   SDL_Event e{};e.type=SDL_KEYDOWN;e.key.keysym.sym=SDLK_ESCAPE;SDL_PushEvent(&e);
 });
 auto start=steady_clock::now();std::string error;
 bool ok=newpipe::run_native_media(window,input,error);events.join();
 auto ms=duration_cast<milliseconds>(steady_clock::now()-start).count();
 assert(ok && input.cancelled->load() && error.empty() && ms<1500);
 SDL_DestroyWindow(window);SDL_Quit();
 std::cout<<"Cancel during blocked opening passed in "<<ms<<" ms\n";
}
