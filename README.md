# sysmon

A Linux `/proc` system monitor with an interactive terminal dashboard.

## Build and run

```sh
make
./sysmon
```

Use a terminal at least 50 columns by 14 rows. Smaller windows show a resize
prompt. The dashboard responds to `SIGWINCH` without waiting for the next metric
refresh. On short terminals, the core list is limited to leave room for at least
one process row; a count indicates when some cores are omitted. Long lines are
clipped rather than wrapped, preventing screen scrolling and duplicate headings.

## Controls

- `q` or Escape: quit
- Up/Down, Page Up/Page Down, Home/End: navigate processes
- `c`: sort by CPU
- `m`: sort by memory
- `p`: sort by PID
- `t`: cycle default, ocean, and monochrome themes

Metrics refresh once per second. Process CPU uses Irix-style percentages:
100% corresponds to one fully utilized core. Disappearing or unreadable process
records are skipped, and reused PIDs receive a new CPU baseline. Selection and
scroll offsets are clamped as processes disappear or the terminal changes size.
`SIGINT` and `SIGTERM` request a normal shutdown so terminal settings and the
primary screen are restored outside the signal handler.

## Validation

```sh
make test
```

The dependency-free Python pseudo-terminal regression test checks single CPU and
process headings, row/column bounds, shrinking and growing windows, the
small-window prompt, theme cycling, navigation, and terminal restoration on
`q`, `SIGINT`, and `SIGTERM`. It requires Python 3 and a Linux pseudo-terminal.

## Performance profiling

```sh
make bench
make bench BENCH_ARGS="--iterations 1000"
make bench BENCH_ARGS="--stage processes --stage render --json"
```

The opt-in microbenchmark measures host, memory, CPU and process collection,
sorting, rendering, and the complete refresh path in isolated subprocesses. It
reports wall-clock latency percentiles, process CPU time, peak RSS, process
count, and generated frame size. Rendering writes to an in-memory counting sink,
so terminal emulator speed is intentionally excluded.

These are diagnostic measurements, not pass/fail tests: `/proc` scanning depends
strongly on the current process count, while timing and peak RSS depend on the
machine, kernel, build flags, and background load. Compare results only under
similar conditions and do not treat one run as a portable performance claim.
