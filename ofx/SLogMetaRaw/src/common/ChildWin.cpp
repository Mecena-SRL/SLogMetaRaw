// SPDX-License-Identifier: GPL-3.0-or-later
// Windows implementation of the child-process API in Child.h (the POSIX one is in Child.cpp).
#ifdef _WIN32
#include "Child.h"

#include <windows.h>
#include <shellapi.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <cwchar>

#include "Files.h"
#include "FlatJson.h"

static const size_t kMaxOutput = 65536;

EnvSnapshot EnvSnapshot::capture()
{
    EnvSnapshot e;
    wchar_t* block = GetEnvironmentStringsW();
    if (!block) return e;
    for (const wchar_t* v = block; *v; v += wcslen(v) + 1) {
        if (_wcsnicmp(v, L"PYTHONPATH=", 11) != 0 && _wcsnicmp(v, L"PYTHONHOME=", 11) != 0) e.vars.push_back(narrow(v));
    }
    FreeEnvironmentStringsW(block);
    return e;
}

int Child::elapsedMs() const
{
    return (int)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count();
}

// CommandLineToArgvW rules: quote when needed, double the backslashes before a quote.
static std::string quoteArg(const std::string& a)
{
    if (!a.empty() && a.find_first_of(" \t\n\v\"") == std::string::npos) return a;
    std::string o = "\"";
    size_t backslashes = 0;
    for (char c : a) {
        if (c == '\\') { ++backslashes; continue; }
        if (c == '"') o.append(backslashes * 2 + 1, '\\');
        else o.append(backslashes, '\\');
        backslashes = 0;
        o += c;
    }
    o.append(backslashes * 2, '\\');
    return o + "\"";
}

bool spawnProcess(const std::vector<std::string>& argv, const EnvSnapshot& env, Child& c, std::string& error,
                  const std::string& stderrPath)
{
    SECURITY_ATTRIBUTES sa = { sizeof(sa), nullptr, TRUE };
    HANDLE rd = nullptr, wr = nullptr;
    if (!CreatePipe(&rd, &wr, &sa, 0)) { error = "pipe non disponibile"; return false; }
    SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);

    HANDLE in = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, nullptr);
    HANDLE err = INVALID_HANDLE_VALUE;
    if (!stderrPath.empty())
        err = CreateFileW(widen(stderrPath).c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_ALWAYS,
                          FILE_ATTRIBUTE_NORMAL, nullptr);
    if (err == INVALID_HANDLE_VALUE)
        err = CreateFileW(L"NUL", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, nullptr);

    std::string cmd;
    for (const std::string& a : argv) cmd += (cmd.empty() ? "" : " ") + quoteArg(a);
    std::wstring cmdBuf = widen(cmd);   // CreateProcessW may write into the command line
    std::wstring envBlock;
    for (const std::string& e : env.vars) envBlock += widen(e) + L'\0';
    envBlock += L'\0';

    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = in;
    si.hStdOutput = wr;
    si.hStdError = err;
    PROCESS_INFORMATION pi = {};
    // CREATE_SUSPENDED: the process joins the job before it can start a grandchild outside it
    const BOOL ok = CreateProcessW(widen(argv[0]).c_str(), &cmdBuf[0], nullptr, nullptr, TRUE,
                                   CREATE_NO_WINDOW | CREATE_SUSPENDED | CREATE_UNICODE_ENVIRONMENT,
                                   env.vars.empty() ? nullptr : &envBlock[0], nullptr, &si, &pi);
    const DWORD lastError = GetLastError();
    CloseHandle(wr);
    CloseHandle(in);
    CloseHandle(err);
    if (!ok) {
        CloseHandle(rd);
        error = "avvio non riuscito (errore " + std::to_string(lastError) + ")";
        return false;
    }
    HANDLE job = CreateJobObjectW(nullptr, nullptr);
    // No KILL_ON_JOB_CLOSE: the "Rileggi" button leaves a detached writer behind on purpose, and closing
    // the handle after a normal exit must not take it down. killProcess still ends the whole job.
    if (job && !AssignProcessToJobObject(job, pi.hProcess)) {   // an empty job would kill nothing
        CloseHandle(job);
        job = nullptr;
    }
    ResumeThread(pi.hThread);
    CloseHandle(pi.hThread);
    c = Child();
    c.pid = pi.dwProcessId;
    c.process = pi.hProcess;
    c.pipe = rd;
    c.job = job;
    c.started = std::chrono::steady_clock::now();
    return true;
}

static void closeHandles(Child& c)
{
    if (c.pipe) { CloseHandle((HANDLE)c.pipe); c.pipe = nullptr; }
    if (c.process) { CloseHandle((HANDLE)c.process); c.process = nullptr; }
    if (c.job) { CloseHandle((HANDLE)c.job); c.job = nullptr; }
}

// Non-blocking: reads what is there; closes the pipe at EOF.
static void drain(Child& c)
{
    char buf[8192];
    while (c.pipe) {
        DWORD avail = 0;
        if (!PeekNamedPipe((HANDLE)c.pipe, nullptr, 0, nullptr, &avail, nullptr)) {   // broken pipe = EOF
            CloseHandle((HANDLE)c.pipe);
            c.pipe = nullptr;
            return;
        }
        if (avail == 0) return;
        DWORD n = 0;
        if (!ReadFile((HANDLE)c.pipe, buf, (DWORD)std::min<size_t>(sizeof(buf), avail), &n, nullptr) || n == 0) return;
        if (c.out.size() < kMaxOutput) c.out.append(buf, std::min((size_t)n, kMaxOutput - c.out.size()));
    }
}

static void checkExit(Child& c)
{
    if (c.exited || !c.process) return;
    if (WaitForSingleObject((HANDLE)c.process, 0) == WAIT_OBJECT_0) {
        DWORD code = 0;
        GetExitCodeProcess((HANDLE)c.process, &code);
        c.exited = true;
        c.exitCode = (int)code;
    }
}

bool waitProcess(Child& c, int timeoutMs, const std::atomic<bool>* cancel)
{
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(std::max(timeoutMs, 0));
    while (true) {
        drain(c);
        checkExit(c);
        if (c.exited) {
            drain(c);
            closeHandles(c);
            return true;
        }
        const int left = (int)std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - std::chrono::steady_clock::now()).count();
        if (left <= 0 || (cancel && cancel->load())) return false;
        if (c.process) WaitForSingleObject((HANDLE)c.process, (DWORD)std::min(left, 20));
        else Sleep(5);
    }
}

void reapStrays() {}   // handles are closed with the process: nothing to collect

void killProcess(Child& c)
{
    if (c.job) TerminateJobObject((HANDLE)c.job, 1);
    if (c.process) TerminateProcess((HANDLE)c.process, 1);
    c.exited = true;
    closeHandles(c);
}

ChildResult runProcess(const std::vector<std::string>& argv, const EnvSnapshot& env, int timeoutMs,
                       const std::atomic<bool>* cancel, const std::string& stderrPath)
{
    ChildResult r;
    Child c;
    if (!spawnProcess(argv, env, c, r.error, stderrPath)) return r;
    r.started = true;
    if (waitProcess(c, timeoutMs, cancel)) {
        r.finished = true;
        r.exitCode = c.exitCode;
    } else {
        r.timedOut = true;
        killProcess(c);
    }
    r.out = c.out;
    return r;
}

static const char* kBootstrap = "import runpy, sys; sys.path.insert(0, sys.argv.pop(1)); "
                                "runpy.run_module('slogmetaraw', run_name='__main__')";

bool findPython(PythonCommand& cmd, std::string& error)
{
    std::string lib;
    if (readFile(supportDir() + "/lib_path", lib)) {
        if (lib.compare(0, 3, "\xEF\xBB\xBF") == 0) lib.erase(0, 3);   // the installer writes UTF-8 with a BOM
        lib = trim(lib);
    }
    if (lib.empty() && fileExists(supportDir() + "/lib/slogmetaraw")) lib = supportDir() + "/lib";
    for (const std::string& c : pythonCandidates()) {
        if (GetFileAttributesW(widen(c).c_str()) != INVALID_FILE_ATTRIBUTES) {
            cmd.python = c;
            cmd.lib = lib;
            return true;
        }
    }
    error = "Python 3 non trovato (Resolve 20 o precedenti: installa Python 3 da python.org)";
    return false;
}

// PEP 514: the python.org installers (and the Microsoft Store one) register every Python 3 under
// Software\Python\PythonCore\<tag>. Newest first; 3.6 is the oldest the library runs on.
static void registeredPythons(std::vector<std::string>& out)
{
    struct Found { long minor; std::string exe; };
    std::vector<Found> found;
    const struct { HKEY root; REGSAM view; } hives[] = {
        { HKEY_CURRENT_USER, 0 }, { HKEY_LOCAL_MACHINE, KEY_WOW64_64KEY }, { HKEY_LOCAL_MACHINE, KEY_WOW64_32KEY } };
    for (const auto& h : hives) {
        HKEY core = nullptr;
        if (RegOpenKeyExW(h.root, L"Software\\Python\\PythonCore", 0, KEY_READ | h.view, &core) != ERROR_SUCCESS)
            continue;
        for (DWORD i = 0;; ++i) {
            wchar_t tag[64];
            DWORD len = 64;
            if (RegEnumKeyExW(core, i, tag, &len, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS) break;
            wchar_t* dot = nullptr;
            const long major = wcstol(tag, &dot, 10);
            const long minor = (dot && *dot == L'.') ? wcstol(dot + 1, nullptr, 10) : -1;
            if (major != 3 || minor < 6) continue;
            const std::wstring sub = std::wstring(tag) + L"\\InstallPath";
            wchar_t buf[1024];
            DWORD size = sizeof(buf);
            std::string exe;
            if (RegGetValueW(core, sub.c_str(), L"ExecutablePath", RRF_RT_REG_SZ, nullptr, buf, &size) == ERROR_SUCCESS) {
                exe = narrow(buf);
            } else {
                size = sizeof(buf);
                if (RegGetValueW(core, sub.c_str(), nullptr, RRF_RT_REG_SZ, nullptr, buf, &size) == ERROR_SUCCESS) {
                    exe = narrow(buf);
                    if (!exe.empty() && exe.back() != '\\' && exe.back() != '/') exe += '\\';
                    if (!exe.empty()) exe += "python.exe";
                }
            }
            if (!exe.empty()) found.push_back({ minor, exe });
        }
        RegCloseKey(core);
    }
    std::stable_sort(found.begin(), found.end(), [](const Found& a, const Found& b) { return a.minor > b.minor; });
    for (const Found& f : found) out.push_back(f.exe);
}

// Resolve 21 ships its own python.exe; Resolve 20 and earlier use the Python 3 the user installed, which the
// python.org installer leaves off the PATH by default. The python.exe in WindowsApps found on the PATH is the
// Microsoft Store alias: without the Store Python it opens the Store instead of running anything.
std::vector<std::string> pythonCandidates()
{
    std::vector<std::string> candidates;
    const std::string forced = envVar("SLOGMETARAW_PYTHON");
    if (!forced.empty()) candidates.push_back(forced);
    const std::string pf = envVar("ProgramFiles");
    candidates.push_back((pf.empty() ? std::string("C:/Program Files") : pf) + "/Blackmagic Design/DaVinci Resolve/python.exe");
    registeredPythons(candidates);
    std::string onPath = findExecutable("python");
    std::string lower = onPath;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char ch) { return (char)tolower(ch); });
    std::replace(lower.begin(), lower.end(), '/', '\\');
    if (lower.find("\\microsoft\\windowsapps\\") != std::string::npos) onPath.clear();
    if (!onPath.empty()) candidates.push_back(onPath);
    return candidates;
}

std::vector<std::string> pythonArgv(const PythonCommand& cmd, const std::vector<std::string>& args)
{
    std::vector<std::string> a = { cmd.python, "-X", "pycache_prefix=" + supportDir() + "/pycache",
                                   "-c", kBootstrap, cmd.lib };
    a.insert(a.end(), args.begin(), args.end());
    return a;
}

std::string childLogPath()
{
    const std::string dir = logDir();
    return makeDirs(dir) ? dir + "/plugin-child.log" : "";
}

bool openUrl(const std::string& url)
{
    return (INT_PTR)ShellExecuteW(nullptr, L"open", widen(url).c_str(), nullptr, nullptr, SW_SHOWNORMAL) > 32;
}

#endif   // _WIN32
