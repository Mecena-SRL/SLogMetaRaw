// SPDX-License-Identifier: GPL-3.0-or-later
// Windows implementation of the child-process API in Child.h (the POSIX one is in Child.cpp).
#ifdef _WIN32
#include "Child.h"

#include <windows.h>
#include <shellapi.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>

#include "Files.h"
#include "FlatJson.h"

static const size_t kMaxOutput = 65536;

EnvSnapshot EnvSnapshot::capture()
{
    EnvSnapshot e;
    char* block = GetEnvironmentStringsA();
    if (!block) return e;
    for (const char* v = block; *v; v += strlen(v) + 1) {
        if (_strnicmp(v, "PYTHONPATH=", 11) != 0 && _strnicmp(v, "PYTHONHOME=", 11) != 0) e.vars.push_back(v);
    }
    FreeEnvironmentStringsA(block);
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

    HANDLE in = CreateFileA("NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, nullptr);
    HANDLE err = INVALID_HANDLE_VALUE;
    if (!stderrPath.empty())
        err = CreateFileA(stderrPath.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_ALWAYS,
                          FILE_ATTRIBUTE_NORMAL, nullptr);
    if (err == INVALID_HANDLE_VALUE)
        err = CreateFileA("NUL", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, nullptr);

    std::string cmd;
    for (const std::string& a : argv) cmd += (cmd.empty() ? "" : " ") + quoteArg(a);
    std::vector<char> cmdBuf(cmd.begin(), cmd.end());
    cmdBuf.push_back('\0');
    std::string envBlock;
    for (const std::string& e : env.vars) envBlock += e + '\0';
    envBlock += '\0';

    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = in;
    si.hStdOutput = wr;
    si.hStdError = err;
    PROCESS_INFORMATION pi = {};
    // CREATE_SUSPENDED: the process joins the job before it can start a grandchild outside it
    const BOOL ok = CreateProcessA(argv[0].c_str(), cmdBuf.data(), nullptr, nullptr, TRUE,
                                   CREATE_NO_WINDOW | CREATE_SUSPENDED, envBlock.empty() ? nullptr : &envBlock[0],
                                   nullptr, &si, &pi);
    const DWORD lastError = GetLastError();
    CloseHandle(wr);
    CloseHandle(in);
    CloseHandle(err);
    if (!ok) {
        CloseHandle(rd);
        error = "avvio non riuscito (errore " + std::to_string(lastError) + ")";
        return false;
    }
    HANDLE job = CreateJobObjectA(nullptr, nullptr);
    if (job) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION li = {};
        li.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        SetInformationJobObject(job, JobObjectExtendedLimitInformation, &li, sizeof(li));
        AssignProcessToJobObject(job, pi.hProcess);
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
    if (c.job) { CloseHandle((HANDLE)c.job); c.job = nullptr; }   // KILL_ON_JOB_CLOSE
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
    else if (c.process) TerminateProcess((HANDLE)c.process, 1);
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
    if (readFile(supportDir() + "/lib_path", lib)) lib = trim(lib);
    if (lib.empty() && fileExists(supportDir() + "/lib/slogmetaraw")) lib = supportDir() + "/lib";
    std::vector<std::string> candidates;
    if (const char* forced = getenv("SLOGMETARAW_PYTHON")) candidates.push_back(forced);
    const char* pf = getenv("ProgramFiles");
    const std::string base = pf ? pf : "C:/Program Files";
    candidates.push_back(base + "/Blackmagic Design/DaVinci Resolve/python.exe");
    char found[MAX_PATH];
    if (SearchPathA(nullptr, "python", ".exe", MAX_PATH, found, nullptr)) candidates.push_back(found);
    for (const std::string& c : candidates) {
        if (GetFileAttributesA(c.c_str()) != INVALID_FILE_ATTRIBUTES) {
            cmd.python = c;
            cmd.lib = lib;
            return true;
        }
    }
    error = "Python non trovato";
    return false;
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
    return (INT_PTR)ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL) > 32;
}

#endif   // _WIN32
