#include "newpipe/native_player.hpp"
#include <SDL2/SDL.h>
#include <thread>
#include <chrono>
#include <cassert>
#include <iostream>
int main(){SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO);auto*w=SDL_CreateWindow("test",0,0,640,360,0);newpipe::NativePlaybackState state;newpipe::NativeMediaInput input;input.url="/tmp/kaitube-native-test/long.mp4";input.state=&state;
std::thread keys([]{auto send=[](SDL_Keycode key){SDL_Event e{};e.type=SDL_KEYDOWN;e.key.keysym.sym=key;SDL_PushEvent(&e);std::this_thread::sleep_for(std::chrono::milliseconds(100));};std::this_thread::sleep_for(std::chrono::milliseconds(200));send(SDLK_x);send(SDLK_DOWN);send(SDLK_RIGHT);send(SDLK_RIGHT);send(SDLK_RIGHT);send(SDLK_SPACE);});
std::string error;bool ok=newpipe::run_native_media(w,input,error);keys.join();assert(ok&&state.restart_at>=0&&state.speed==1.75);SDL_DestroyWindow(w);SDL_Quit();std::cout<<"Selection applied once at 1.75x: passed\n";}
