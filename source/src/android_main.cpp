// android_main.cpp: Android entry point for AssaultCube.
//
// The game data (config/, packages/, bot/, mods/) ships inside the APK as assets.
// AssaultCube reads its data with plain stdio, so on first launch (and after every
// update) the assets listed in assets/manifest.txt are copied to internal storage.
// Then we chdir there and start the normal game with a private profile directory.

#ifdef __ANDROID__

#include <SDL.h>
#include <SDL_main.h>
#include <android/log.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "AssaultCube", __VA_ARGS__)

extern int ac_main(int argc, char **argv);

static void mkdirs(const std::string &path)
{
    std::string cur;
    size_t pos = 0;
    while(pos != std::string::npos)
    {
        pos = path.find('/', pos + 1);
        cur = path.substr(0, pos);
        if(!cur.empty()) mkdir(cur.c_str(), 0755);
    }
}

static bool readasset(const char *name, std::vector<char> &out)
{
    SDL_RWops *rw = SDL_RWFromFile(name, "rb");
    if(!rw) return false;
    out.clear();
    char buf[65536];
    size_t n;
    while((n = SDL_RWread(rw, buf, 1, sizeof(buf))) > 0) out.insert(out.end(), buf, buf + n);
    SDL_RWclose(rw);
    return true;
}

static bool copyasset(const std::string &name, const std::string &dest)
{
    SDL_RWops *rw = SDL_RWFromFile(name.c_str(), "rb");
    if(!rw) { LOGI("missing asset %s", name.c_str()); return false; }
    size_t slash = dest.rfind('/');
    if(slash != std::string::npos) mkdirs(dest.substr(0, slash));
    FILE *f = fopen(dest.c_str(), "wb");
    if(!f) { SDL_RWclose(rw); LOGI("cannot write %s: %s", dest.c_str(), strerror(errno)); return false; }
    static char buf[1 << 18];
    size_t n;
    while((n = SDL_RWread(rw, buf, 1, sizeof(buf))) > 0) fwrite(buf, 1, n, f);
    fclose(f);
    SDL_RWclose(rw);
    return true;
}

static std::string readfile(const std::string &path)
{
    std::string s;
    FILE *f = fopen(path.c_str(), "rb");
    if(!f) return s;
    char buf[256];
    size_t n;
    while((n = fread(buf, 1, sizeof(buf), f)) > 0) s.append(buf, n);
    fclose(f);
    return s;
}

static void extractdata(const std::string &base)
{
    std::vector<char> manifest;
    if(!readasset("manifest.txt", manifest)) { LOGI("no manifest in assets"); return; }
    std::string text(manifest.begin(), manifest.end());
    size_t eol = text.find('\n');
    std::string version = text.substr(0, eol);
    std::string stamp = base + "/.datastamp";
    if(readfile(stamp) == version) { LOGI("data up to date (%s)", version.c_str()); return; }

    LOGI("extracting game data %s", version.c_str());
    int count = 0;
    size_t pos = eol == std::string::npos ? text.size() : eol + 1;
    while(pos < text.size())
    {
        size_t next = text.find('\n', pos);
        std::string name = text.substr(pos, next == std::string::npos ? std::string::npos : next - pos);
        pos = next == std::string::npos ? text.size() : next + 1;
        if(!name.empty() && name[name.size() - 1] == '\r') name.erase(name.size() - 1);
        if(name.empty()) continue;
        if(copyasset(name, base + "/" + name)) count++;
    }
    LOGI("extracted %d files", count);
    FILE *f = fopen(stamp.c_str(), "wb");
    if(f) { fwrite(version.data(), 1, version.size(), f); fclose(f); }
}

int main(int argc, char **argv)
{
    const char *internal = SDL_AndroidGetInternalStoragePath();
    if(!internal) { LOGI("no internal storage path"); return 1; }
    std::string base = std::string(internal) + "/game";
    std::string home = std::string(internal) + "/profile";
    mkdirs(base);
    mkdirs(home);
    extractdata(base);
    if(chdir(base.c_str()) != 0) { LOGI("chdir failed: %s", strerror(errno)); return 1; }

    static std::string homearg = "--home=" + home;
    static char *args[] = { (char *)"assaultcube", (char *)homearg.c_str(), (char *)"--init", NULL };
    return ac_main(3, args);
}

#endif // __ANDROID__
