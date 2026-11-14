#include "../src/common/auth_store.cpp"
#include <cassert>
#include <filesystem>
#include <unistd.h>
namespace newpipe { void logf(const char*, ...) {} void log_line(const std::string&) {} }
bool fail_commit = false;
int sd_rename(const char* from, const char* to) {
    if (fail_commit && std::string(from) == "sd-session.tmp") { errno=EIO; return -1; }
    if (std::filesystem::exists(to)) { errno=EEXIST; return -1; }
    return std::rename(from,to);
}
int main() {
 using namespace newpipe;
 std::string err;
 assert(is_google_cookie_domain(".youtube.com"));
 assert(!is_google_cookie_domain("youtube.com.evil.example"));
 assert(!is_google_cookie_domain("notyoutube.com"));
 auto n=parse_auth_source("# Netscape HTTP Cookie File\n#HttpOnly_.youtube.com\tTRUE\t/\tTRUE\t0\tSAPISID\ttest-only\n", "test", "", &err);
 assert(n && n->sapisid=="test-only");
 auto a=parse_auth_source(R"([{"domain":".youtube.com","name":"SAPISID","value":"test-only"}])", "test", "", &err);
 assert(a && a->authenticated());
 assert(!parse_auth_source("SAPISID=test\r\nInjected: value", "test", "", &err));
 auto dir=std::filesystem::temp_directory_path()/("kaitube-auth-"+std::to_string(getpid()));
 std::filesystem::create_directory(dir);std::filesystem::current_path(dir);
 assert(write_text_file("sd-session", "old", sd_rename));
 assert(write_text_file("sd-session", "new", sd_rename));
 std::string saved;assert(read_text_file("sd-session", saved) && saved=="new");
 fail_commit=true;
 assert(!write_text_file("sd-session", "broken", sd_rename));
 assert(read_text_file("sd-session", saved) && saved=="new");
 fail_commit=false;
 auto& store=AuthStore::instance();
 assert(store.update_from_cookie_header("SAPISID=first", "test", &err));
 assert(store.reload(&err) && store.load(&err));assert(store.session().sapisid=="first");
 // Force temporary-file failure; previous session must survive in memory and disk.
 std::filesystem::create_directory(default_auth_session_path()+".tmp");
 assert(!store.update_from_cookie_header("SAPISID=second", "test", &err));
 assert(store.session().sapisid=="first");
 assert(store.reload(&err) && store.load(&err));assert(store.session().sapisid=="first");
 std::filesystem::remove(default_auth_session_path()+".tmp");
 // Simulate interruption after moving the old session to backup.
 assert(std::rename(default_auth_session_path().c_str(), (default_auth_session_path()+".bak").c_str())==0);
 assert(store.reload(&err) && store.load(&err));assert(store.session().sapisid=="first");
 assert(store.clear(&err));assert(!store.has_session());
 assert(!std::filesystem::exists(default_auth_session_path()));
 assert(!std::filesystem::exists(default_auth_session_path()+".bak"));
 assert(store.reload(&err) && store.load(&err) && !store.has_session());
 std::filesystem::current_path(dir.parent_path());std::filesystem::remove_all(dir);
 puts("Cookie import, domain filtering, header validation, save rollback, reload and logout passed");
}
