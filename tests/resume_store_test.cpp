#include "newpipe/resume_store.hpp"
#include <filesystem>
#include <cassert>
#include <iostream>
#include <unistd.h>
int main(){
 auto path=std::filesystem::temp_directory_path()/("kaitube-resume-test-"+std::to_string(getpid()));
 std::filesystem::create_directories(path);std::filesystem::current_path(path);
 using namespace newpipe;
 assert(resume_position("a")==0);assert(save_resume("a",515,1000));
 assert(resume_position("a")==515);assert(save_resume("a",600,1000));assert(resume_position("a")==600);
 assert(save_resume("a",995,1000));assert(resume_position("a")==0);
 assert(save_resume("b",33,100));assert(save_resume("b",0,0));assert(resume_position("b")==0);
 std::filesystem::remove_all(path);std::cout<<"Resume save/reload/restart/completion passed\n";
}
