#include "newpipe/native_player.hpp"
#include <SDL2/SDL.h>
#include <cassert>
#include <iostream>
int main(int argc,char**argv){
 assert(argc==2);assert(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO)==0);
 auto* w=SDL_CreateWindow("resume",0,0,640,360,0);assert(w);
 newpipe::NativeMediaInput input;newpipe::NativePlaybackState state;state.start=1.5;
 input.url=argv[1];input.state=&state;bool prepared=false;
 input.prepare_seek=[&](double t,double duration){assert(t==1.5 && duration>3);prepared=true;return false;};
 std::string error;assert(newpipe::run_native_media(w,input,error));
 assert(prepared && state.start==0 && error.empty());SDL_DestroyWindow(w);SDL_Quit();
 std::cout<<"Incomplete-cache resume guard safely restarted at beginning\n";
}
