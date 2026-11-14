#include "newpipe/native_player.hpp"
#include <SDL2/SDL.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <cassert>
#include <iostream>
#include <vector>
using namespace std::chrono;
static void check(const char* file,std::vector<SDL_Keycode> keys,unsigned seeks,double expected,bool quick=false){
 assert(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO)==0);auto*w=SDL_CreateWindow("player",0,0,640,360,0);assert(w);
 newpipe::NativePlaybackState state;newpipe::NativeMediaInput input;input.url=file;input.state=&state;input.quality="360p";
 std::atomic_bool finished{false};
 std::thread events([&]{std::this_thread::sleep_for(milliseconds(250));
  for(auto key:keys){if(finished)break;SDL_Event e{};e.type=SDL_KEYDOWN;e.key.keysym.sym=key;if(key==SDLK_F1){e.type=SDL_MOUSEBUTTONDOWN;e.button.button=SDL_BUTTON_LEFT;e.button.x=40;e.button.y=322;}SDL_PushEvent(&e);std::this_thread::sleep_for(milliseconds(250));}
  for(int i=0;i<15 && !finished;++i)std::this_thread::sleep_for(milliseconds(100));
  if(!finished){SDL_Event e{};e.type=SDL_QUIT;SDL_PushEvent(&e);}
 });
 auto begin=steady_clock::now();std::string error;bool ok=newpipe::run_native_media(w,input,error);auto elapsed=duration_cast<milliseconds>(steady_clock::now()-begin).count();
 finished=true;events.join();if(!ok)std::cerr<<error<<"\n";
 assert(ok && error.empty() && state.restart_at<0 && state.requested_height<0);
 assert(state.seek_count==seeks);
 if(seeks)assert(state.last_seek>=expected && state.last_seek<expected+3);
 if(quick)assert(elapsed<1300);
 SDL_DestroyWindow(w);SDL_Quit();
}
int main(int argc,char**argv){assert(argc==2);
 // Pausing/resuming hides controls immediately; the next B exits.
 check(argv[1],{SDLK_SPACE,SDLK_SPACE,SDLK_ESCAPE},0,0,true);
 // First B hides controls, second exits. Same behavior from the options sheet.
 check(argv[1],{SDLK_ESCAPE,SDLK_ESCAPE},0,0,true);
 check(argv[1],{SDLK_ESCAPE,SDLK_F1,SDLK_F1,SDLK_SPACE,SDLK_ESCAPE},0,0);
 check(argv[1],{SDLK_x,SDLK_ESCAPE,SDLK_ESCAPE},0,0,true);
 // Normal seeks stay inside the existing player, rather than returning restart_at.
 check(argv[1],{SDLK_e,SDLK_SPACE,SDLK_ESCAPE,SDLK_ESCAPE},1,30);
 check(argv[1],{SDLK_RIGHT,SDLK_SPACE,SDLK_RIGHT,SDLK_SPACE,SDLK_ESCAPE,SDLK_ESCAPE},2,20);
 // Backward seek and paused seek retain a functioning player/audio sink.
 check(argv[1],{SDLK_e,SDLK_SPACE,SDLK_q,SDLK_SPACE,SDLK_ESCAPE,SDLK_ESCAPE},2,0);
 check(argv[1],{SDLK_SPACE,SDLK_RIGHT,SDLK_SPACE,SDLK_ESCAPE,SDLK_ESCAPE},1,10);
 std::cout<<"HUD A/B, options close, in-place forward/backward/repeated/paused seeks passed\n";
}
