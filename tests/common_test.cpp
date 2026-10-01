// SPDX-License-Identifier: GPL-3.0-or-later
// Command-line access to src/common for the Python tests. One command per invocation:
//   cachekey PATH            cache record path the plugin uses for a clip
//   run TIMEOUT_MS ARGV...   spawn a child; prints finished/timedOut/exit/signal/bytes/elapsed
//   which NAME               first executable of that name on PATH (empty line if none)
//   runout TIMEOUT_MS ARGV...  spawn a child and print what it wrote on stdout, nothing else
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "../ofx/SLogMetaRaw/src/common/Child.h"
#include "../ofx/SLogMetaRaw/src/common/Files.h"

int main(int argc, char** argv)
{
    if (argc >= 3 && !strcmp(argv[1], "cachekey")) {
        printf("%s\n", cacheRecordPath(argv[2]).c_str());
        return 0;
    }
    if (argc >= 4 && !strcmp(argv[1], "run")) {
        std::vector<std::string> args(argv + 3, argv + argc);
        Child probe;
        probe.started = std::chrono::steady_clock::now();
        ChildResult r = runProcess(args, EnvSnapshot::capture(), atoi(argv[2]));
        printf("started=%d finished=%d timedOut=%d exit=%d signal=%d bytes=%zu elapsed=%d\n", r.started, r.finished,
               r.timedOut, r.exitCode, r.termSignal, r.out.size(), probe.elapsedMs());
        return 0;
    }
    if (argc >= 4 && !strcmp(argv[1], "runout")) {
        std::vector<std::string> args(argv + 3, argv + argc);
        ChildResult r = runProcess(args, EnvSnapshot::capture(), atoi(argv[2]));
        if (!r.started) { fprintf(stderr, "non avviato: %s\n", r.error.c_str()); return 1; }
        fwrite(r.out.data(), 1, r.out.size(), stdout);
        return 0;
    }
    if (argc >= 3 && !strcmp(argv[1], "which")) {
        printf("%s\n", findExecutable(argv[2]).c_str());
        return 0;
    }
    fprintf(stderr, "uso: common_test cachekey PATH | run TIMEOUT_MS ARGV... | runout TIMEOUT_MS ARGV...\n");
    return 2;
}
