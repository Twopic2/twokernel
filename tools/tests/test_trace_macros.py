from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]


@unittest.skipUnless(shutil.which("clang++"), "clang++ is required")
class TraceMacroTests(unittest.TestCase):
    def run_cpp(self, source, enabled):
        with tempfile.TemporaryDirectory() as directory:
            directory = Path(directory)
            program = directory / "test.cpp"
            executable = directory / "test"
            program.write_text(source)
            command = ["clang++", "-std=c++20", "-O0", "-Wall", "-Wextra", "-Werror",
                       "-I", str(ROOT / "src/include"), str(program),
                       str(ROOT / "src/stats/trace_scope.cpp"), "-o", str(executable)]
            if enabled:
                command.append("-DTWOKERNEL_ENABLE_TRACE")
            result = subprocess.run(command, capture_output=True, text=True, timeout=30)
            self.assertEqual(result.returncode, 0, result.stderr)
            result = subprocess.run([str(executable)], capture_output=True, text=True, timeout=10)
            self.assertEqual(result.returncode, 0, result.stderr)

    def test_disabled_macros_discard_arguments_even_without_optimization(self):
        self.run_cpp(r'''
#include "stats/trace.hpp"
int main() {
    // These arguments must disappear before type checking or linking.
    TK_TRACE_COUNTER(undefined_name, undefined_function());
    TK_TRACE_INTERVAL(undefined_name, undefined_tid, undefined_start, undefined_duration);
    TK_TRACE_SCOPE(undefined_name);
    TK_TRACE_FUNCTION();
    int evaluations = 0;
    TK_TRACE_COUNTER((++evaluations, "counter"), ++evaluations);
    TK_TRACE_INTERVAL((++evaluations, "interval"), ++evaluations,
                      ++evaluations, ++evaluations);
    TK_TRACE_SCOPE((++evaluations, "scope"));
    if (true) TK_TRACE_COUNTER("branch", ++evaluations); else return 1;
    return evaluations;
}
''', enabled=False)

    def test_enabled_macros_and_real_scope_lifetimes(self):
        self.run_cpp(r'''
#include "stats/trace.hpp"
#include <cassert>
#include <string>
#include <type_traits>
#include <vector>

struct Event {
    std::string name;
    std::uint32_t tid;
    std::uint64_t start;
    std::uint64_t value;
};
static std::vector<Event> events;
static bool active = true;
static std::uint64_t now = 1000;
namespace Stats::Trace {
    bool enabled() { return active; }
    std::uint64_t timestamp_us() { return now; }
    Detail::ScopeContext Detail::scope_context() { return {0x10007, now}; }
    void counter(std::string_view name, std::uint64_t time, std::uint64_t value) {
        events.push_back({std::string(name), 0, time, value});
    }
    void interval(std::string_view name, std::uint32_t tid,
                  std::uint64_t start, std::uint64_t duration) {
        events.push_back({std::string(name), tid, start, duration});
    }
}

static void early_return() {
    TK_TRACE_FUNCTION();
    now += 2000;
    return;
}

int main() {
    static_assert(!std::is_copy_constructible_v<Stats::Trace::Scope>);
    static_assert(!std::is_move_constructible_v<Stats::Trace::Scope>);
    int names = 0, values = 0;
    if (true) TK_TRACE_COUNTER((++names, "count"), ++values); else return 1;
    assert(names == 1 && values == 1);
    assert(events.back().name == "count" && events.back().value == 1);
    TK_TRACE_INTERVAL("explicit", 9, 10, 20);
    assert(events.back().tid == 9 && events.back().start == 10 && events.back().value == 20);
    events.clear();

    {
        TK_TRACE_SCOPE("outer");
        now = 2000;
        { TK_TRACE_SCOPE("inner"); now = 3000; }
        now = 5000;
    }
    assert(events.size() == 2);
    assert(events[0].name == "inner" && events[0].start == 2000 && events[0].value == 1000);
    assert(events[1].name == "outer" && events[1].start == 1000 && events[1].value == 4000);
    assert(events[1].tid == 0x10007);
    early_return();
    assert(events.back().name == "early_return" && events.back().value == 2000);

    char name[] = "original";
    { TK_TRACE_SCOPE((++names, name)); name[0] = 'X'; now += 1000; }
    assert(names == 2 && events.back().name == "original");
    {
        TK_TRACE_SCOPE("same line A"); TK_TRACE_SCOPE("same line B");
        now += 1000;
    }
    assert(events.back().name == "same line A");

    const auto previous_size = events.size();
    active = false;
    int tids = 0, starts = 0, durations = 0;
    TK_TRACE_COUNTER((++names, "inactive"), ++values);
    TK_TRACE_INTERVAL((++names, "inactive"), ++tids, ++starts, ++durations);
    { TK_TRACE_SCOPE((++names, "inactive")); TK_TRACE_FUNCTION(); }
    assert(names == 2 && values == 1 && events.size() == previous_size);
    assert(tids == 0 && starts == 0 && durations == 0);
}
''', enabled=True)


if __name__ == "__main__":
    unittest.main()
