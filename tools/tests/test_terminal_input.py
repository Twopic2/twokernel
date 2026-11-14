from pathlib import Path
import os
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]


@unittest.skipUnless(shutil.which("clang++"), "clang++ is required")
class TerminalInputTests(unittest.TestCase):
    def test_ps2_translation_and_tty(self):
        # Only privileged I/O, interrupt masking, and the display are replaced.
        # Compile and exercise the actual decoder, translator, and TTY together.
        with tempfile.TemporaryDirectory() as directory:
            directory = Path(directory)
            stubs = {
                "util/kernel_logger.hpp": """#pragma once
namespace Util { void klog_panic(const char*); }
""",
                "util/spinlocks.hpp": """#pragma once
#include <cassert>
namespace Util {
inline unsigned locks_held = 0;
struct IrqSpinLock {
    bool held = false;
    void acquire() { assert(!held); held = true; ++locks_held; }
    void release() { assert(held); held = false; --locks_held; }
};
}
""",
                "arch/x86-64/arch/io.hpp": """#pragma once
#include <cstdint>
namespace x86::IO {
inline std::uint8_t inb(std::uint16_t) { return 0; }
inline void outb(std::uint16_t, std::uint8_t) {}
}
""",
            }
            for name, source in stubs.items():
                path = directory / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(source)
            executable = directory / "terminal_input"
            command = [
                "clang++", "-std=c++20", "-Wall", "-Wextra", "-Werror",
                "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
                "-I", str(directory), "-I", str(ROOT / "src/include"),
                "-I", str(ROOT / "dependencies/flanterm/src"),
                "-I", str(ROOT / "dependencies/limine-proto/include"),
                str(ROOT / "tools/tests/terminal_input.cpp"),
                str(ROOT / "src/kernel/drivers/ps2.cpp"),
                str(ROOT / "src/kernel/drivers/keyboard.cpp"),
                str(ROOT / "src/kernel/drivers/fbtty.cpp"),
                "-o", str(executable),
            ]
            result = subprocess.run(command, capture_output=True, text=True, timeout=30)
            self.assertEqual(result.returncode, 0, result.stderr)
            # LSan uses ptrace, which is unavailable in sandboxed host tests.
            environment = dict(os.environ)
            environment["ASAN_OPTIONS"] = environment.get("ASAN_OPTIONS", "") + ":detect_leaks=0"
            result = subprocess.run([str(executable)], capture_output=True, text=True,
                                    timeout=10, env=environment)
            self.assertEqual(result.returncode, 0, result.stderr)


if __name__ == "__main__":
    unittest.main()
