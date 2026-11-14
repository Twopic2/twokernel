# Note

There was some use of generative Ai being used to create the catch2-clone and setting up any analyzers. This is not a perfect example. 

# Tooling

Run these commands from the repository root. This guide covers building and
running TwoKernel, kernel tests, QEMU/GDB debugging, and Perfetto profiling.

## Build and run

```sh
./run.sh
./run.sh --release
```

The default is a Debug build. The script configures the selected CMake build,
builds the ISO, and boots it in QEMU with 256 MiB of RAM.

| Option | Purpose |
| --- | --- |
| `--debug` | Select the Debug build, also the default. |
| `--release` | Select the Release build. |
| `--gdb` | Start QEMU paused with its GDB server on port 1234. |
| `--intlog` | Write QEMU interrupt and exception diagnostics to `qemu.log`. |
| `--trace` | Enable kernel trace capture and convert it when QEMU exits. |

Options can be combined, for example `./run.sh --debug --gdb --trace`.

To build the Debug ISO without starting QEMU:

```sh
cmake --preset general-debug-build
cmake --build build/general-debug-build --target iso
```

Use the `general-release-build` preset and matching build directory for Release.
Build artifacts stay under `build/<preset>/`.

The ISO also includes `user_init.elf`, built from [src/user](src/user). Its
`_start` calls a minimal `main` and exits with status 0. Running it requires
the user memory and ELF loading startup path; `kmain` does not launch it yet.

## Testing

### Kernel tests

Kernel suites live in [src/kernel/tests](src/kernel/tests) and use the
[KTest harness](src/include/tests/ktest.hpp). Select a suite through calls in
[src/kmain.cpp](src/kmain.cpp), where examples are currently commented out:

```cpp
KTest::list();                 // List registered tests.
KTest::run("[deque]");         // Run tests matching this tag.
KTest::run("[deque]", true);   // Print each test's result as well.
```

Place calls after global constructors and framebuffer logging are initialized,
and after any subsystem the selected suite needs. Then rebuild and boot with
`./run.sh`. Results appear on the kernel's framebuffer terminal.

Available tags are `[pmm]`, `[vmm]`, `[heap]`, `[scheduler]`, `[deque]`, `[irq]`,
`[ringbuffer]`, `[keyboard]`, `[elf]`, `[syscall]`, and `[spinlock]`.

Add new cases with `TEST_CASE`, `CHECK`, and `REQUIRE`. Add each new test source to
`KERNEL_SOURCES` in [CMakeLists.txt](CMakeLists.txt) so its cases are registered.

## Debugging

### GDB with QEMU

Start a Debug build with QEMU waiting for a debugger:

```sh
./run.sh --gdb
```

In a second terminal at the repository root:

```sh
gdb build/general-debug-build/twokernel.elf
```

Then connect, set a breakpoint, and resume boot:

```gdb
target remote localhost:1234
break kmain
continue
```

The ELF supplies symbols and source information; QEMU runs the kernel from the
ISO. Useful commands after stopping include:

```gdb
bt
info registers
x/10i $rip
break Memory::Pmm::alloc
watch Memory::Pmm::g_ref_alloc
continue
```

### Interrupt and exception logs

```sh
./run.sh --intlog
```

Inspect `qemu.log` in the repository root for QEMU's interrupt and exception
activity. Combine `--intlog` with `--gdb` to capture diagnostics while debugging.
This log is separate from the kernel's framebuffer output and the Perfetto trace.

## Profiling with Perfetto

Run a capture with:

```sh
./run.sh --trace
```

The kernel records bounded events in memory. A `trace exporter` kernel thread
samples memory every 100 ms and drains events to QEMU's debug console on port
`0xe9`. Closing QEMU converts `build/general-debug-build/trace.jsonl` into
`build/general-debug-build/trace.json`. Open that JSON file with **Open trace file**
at https://ui.perfetto.dev. Python 3 is required for conversion.

Use `./run.sh --release --trace` for a Release capture (files go in
`build/general-release-build`). Running without `--trace` disables event capture
and does not start the exporter thread.

The trace contains:

- PMM managed, allocated, and peak allocated bytes, plus allocation failures.
- Heap requested and backing bytes, peak backing bytes, live allocations, and failures.
- `Running` and `Ready` intervals on each thread's track, including the exporter.
- A `Trace dropped events` counter for diagnosing buffer exhaustion.

Heap backing is included in PMM allocations; those totals should not be added
together. PMM counters cover allocator-managed memory, excluding reserved ranges
such as the kernel image. Peaks are updated on every allocation, even between
samples. The counters include allocations made during boot, before tracing starts.

The scheduler uses the PIT clock, so timestamps have **1 ms resolution**, despite
being exported in microseconds. Running intervals describe scheduler ownership of
the CPU and include interrupt time. They do not measure context-switch execution
cost. These are custom Perfetto tracks, rather than native Linux scheduling events.
The exporter consumes CPU time and appears in the trace itself.

To add your own instrumentation, include `stats/trace.hpp`:

```cpp
#include "stats/trace.hpp"

void load_something(std::uint64_t object_count) {
    TK_TRACE_FUNCTION(); // Labels the enclosing function with __func__.
    TK_TRACE_COUNTER("Objects loaded", object_count);
    {
        TK_TRACE_SCOPE("Read data"); // Ends on block exit, including early return.
        // ...
    }
}

// For intervals whose start and duration you already measured (microseconds):
void record_operation(std::uint32_t tid, std::uint64_t start_us,
                      std::uint64_t duration_us) {
    TK_TRACE_INTERVAL("My operation", tid, start_us, duration_us);
}
```

These macros are enabled by `./run.sh --trace`, which sets the build definition
`TWOKERNEL_ENABLE_TRACE`. Without `--trace`, they expand to no-ops and do not
evaluate their arguments. For larger tracing-only blocks, use
`#ifdef TWOKERNEL_ENABLE_TRACE`.

`TK_TRACE_SCOPE` and `TK_TRACE_FUNCTION` record elapsed wall time on a separate
`Scopes: <thread name>` track, so scopes spanning preemption do not overlap the
thread's scheduler intervals. Names are copied at scope entry; nested scopes and
early returns are supported. Scopes include time spent preempted or blocked.
The PIT resolution still applies: scopes shorter than a tick may produce no slice.
Use explicit intervals on a separate custom track when timing work across threads;
an automatic scope belongs to the thread that entered it.

Producer calls copy names into a 1,024-event buffer without allocating or writing
to the console. Names are limited to 47 UTF-8 bytes. A full buffer drops new
events and increments its loss counter. Only the exporter calls `flush()`;
formatting and output happen outside allocator and scheduler locks. The current
scheduler/profiling integration targets the existing single-CPU kernel.

You can also convert captures manually, including ones interrupted mid-record:

```sh
python3 tools/trace_to_perfetto.py build/general-debug-build/trace.jsonl
```

Conversion preserves complete records and ignores an incomplete final record.
Malformed complete records fail conversion. A forced stop can lose buffered events
and the final running interval; a reported loss count makes detailed reconstruction
incomplete.

