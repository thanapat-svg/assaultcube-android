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
#include <signal.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <unwind.h>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "AssaultCube", __VA_ARGS__)

extern int ac_main(int argc, char **argv);
extern "C" void ac_flushlog();

// ---------------------------------------------------------------- crash reporter
// On a crash, write the signal and a backtrace to <profile>/crash.txt.
// On the next start that report (plus the end of the previous log) is shown in a dialog.

static char crashpath[512];
static struct sigaction oldactions[NSIG];

static void writestr(int fd, const char *s) { write(fd, s, strlen(s)); }
static void writehex(int fd, uintptr_t v)
{
    char buf[2 + 2 * sizeof(v) + 1];
    buf[0] = '0'; buf[1] = 'x';
    for(int i = 0; i < (int)(2 * sizeof(v)); i++) buf[2 + i] = "0123456789abcdef"[(v >> (4 * (2 * sizeof(v) - 1 - i))) & 0xF];
    buf[sizeof(buf) - 1] = 0;
    writestr(fd, buf);
}
static void writeint(int fd, int v)
{
    char buf[16]; int n = 0; bool neg = v < 0; unsigned u = neg ? -v : v;
    do { buf[n++] = '0' + u % 10; u /= 10; } while(u && n < 15);
    if(neg) writestr(fd, "-");
    while(n) write(fd, &buf[--n], 1);
}

struct btstate { uintptr_t pcs[32]; int n; };
static _Unwind_Reason_Code btcallback(struct _Unwind_Context *ctx, void *arg)
{
    btstate *st = (btstate *)arg;
    uintptr_t pc = _Unwind_GetIP(ctx);
    if(pc && st->n < 32) st->pcs[st->n++] = pc;
    return st->n < 32 ? _URC_NO_REASON : _URC_END_OF_STACK;
}

static void crashhandler(int sig, siginfo_t *info, void *uctx)
{
    int fd = open(crashpath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if(fd >= 0)
    {
        writestr(fd, "signal "); writeint(fd, sig);
        writestr(fd, " code "); writeint(fd, info ? info->si_code : 0);
        writestr(fd, " addr "); writehex(fd, info ? (uintptr_t)info->si_addr : 0);
        writestr(fd, "\n");
        btstate st; st.n = 0;
        _Unwind_Backtrace(btcallback, &st);
        for(int i = 0; i < st.n; i++)
        {
            Dl_info di;
            writestr(fd, "#"); writeint(fd, i); writestr(fd, " ");
            if(dladdr((void *)st.pcs[i], &di) && di.dli_fname)
            {
                const char *lib = strrchr(di.dli_fname, '/');
                writestr(fd, lib ? lib + 1 : di.dli_fname);
                writestr(fd, "+"); writehex(fd, st.pcs[i] - (uintptr_t)di.dli_fbase);
                if(di.dli_sname) { writestr(fd, " "); writestr(fd, di.dli_sname); }
            }
            else writehex(fd, st.pcs[i]);
            writestr(fd, "\n");
        }
        close(fd);
    }
    ac_flushlog();
    // hand over to the system handler so Android still records the crash and the process dies
    sigaction(sig, &oldactions[sig], NULL);
    raise(sig);
}

static void installcrashhandler(const std::string &home)
{
    snprintf(crashpath, sizeof(crashpath), "%s/crash.txt", home.c_str());
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = crashhandler;
    sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
    sigemptyset(&sa.sa_mask);
    const int sigs[] = { SIGSEGV, SIGABRT, SIGBUS, SIGFPE, SIGILL };
    for(int s : sigs) sigaction(s, &sa, &oldactions[s]);
}

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

static std::string readfile(const std::string &path);

static void showlastcrash(const std::string &home)
{
    std::string report = readfile(home + "/crash.txt");
    if(report.empty()) return;
    remove((home + "/crash.txt").c_str());
    std::string log = readfile(home + "/clientlog.txt");
    // keep the last 15 lines of the previous log
    int lines = 0;
    size_t pos = log.size();
    while(pos > 0 && lines < 16) { pos = log.rfind('\n', pos - 1); if(pos == std::string::npos) { pos = 0; break; } lines++; }
    std::string msg = "AssaultCube crashed last time. Please send a screenshot of this to the developer.\n\n" + report +
                      "\nlast log lines:\n" + log.substr(pos);
    LOGI("%s", msg.c_str());
    remove((home + "/lastcrash.txt").c_str());
    FILE *f = fopen((home + "/lastcrash.txt").c_str(), "wb");
    if(f) { fwrite(msg.data(), 1, msg.size(), f); fclose(f); }
    SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "AssaultCube crash report", msg.c_str(), NULL);
}

int main(int argc, char **argv)
{
    const char *internal = SDL_AndroidGetInternalStoragePath();
    if(!internal) { LOGI("no internal storage path"); return 1; }
    std::string base = std::string(internal) + "/game";
    std::string home = std::string(internal) + "/profile";
    mkdirs(base);
    mkdirs(home);
    installcrashhandler(home);
    showlastcrash(home);
    extractdata(base);
    if(chdir(base.c_str()) != 0) { LOGI("chdir failed: %s", strerror(errno)); return 1; }

    static std::string homearg = "--home=" + home;
    static char *args[] = { (char *)"assaultcube", (char *)homearg.c_str(), (char *)"--init", NULL };
    return ac_main(3, args);
}

#endif // __ANDROID__
