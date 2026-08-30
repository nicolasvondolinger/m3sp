# Multi-period, Multi-rate and Multi-channels with variable bandwidth Scheduling Problem (M3SP)

A BRKGA (Biased Random-Key Genetic Algorithm) for scheduling wireless links over
time slots, spectrum bands and channels of variable bandwidth, maximising total
throughput under co-channel interference.

## Building

```bash
cmake -S . -B build
cmake --build build -j
```

Requires a C++17 compiler with OpenMP. The default build type is `Release`
(`-O3`), matching the old `src/Makefile`.

| Option | Default | Effect |
| --- | --- | --- |
| `M3SP_BUILD_TESTS` | `ON` | Build the unit tests in `tests/` |
| `M3SP_ENABLE_WARNINGS` | `OFF` | Compile with a strict warning set |
| `M3SP_ENABLE_PROFILING` | `OFF` | Also build `m3sp_prof` (`-pg`) and the `profile-report` target |

## Running

The binary resolves `../instances` and `../output` against the working
directory, so run it from the build tree:

```bash
cd build
./m3sp <type> <instance-set>      # e.g. ./m3sp gcsid_plus vrbsp
```

Run it with no arguments to list both. The instance set is any subdirectory of
`instances/` (currently `vrbsp` and `md_vrbsp`); it is discovered at run time,
so adding a set needs no recompile. Every `.txt` file in the chosen set is
solved in turn.

The type selects the decoding heuristic:

| Type | Decoder |
| --- | --- |
| `mic` | Two-phase constructive heuristic |
| `dp` | `mic` followed by the DP channel-split pass |
| `gcsid` | Best-channel insertion plus channel fusion |
| `gcsid_plus` | `gcsid` with tie-broken ordering, allocation-free candidate pricing and a greedy split |
| `local_search` | `gcsid` plus a relocation local search |

Results land in `output/<instance-set>/output_<type>_<pe>/<n>/`, so runs of
different sets or types never overwrite each other. Each run writes
`solution.txt`, `objectives.txt`, `time.txt`, `population.txt` and
`channels.txt` there.

## Testing

Add a `test_<something>.cpp` file under `tests/`; it is compiled into its own
executable, linked against `m3sp_core` and registered with CTest automatically.
A test passes when `main` returns 0.

```bash
ctest --test-dir build --output-on-failure
```

## Profiling

```bash
cmake -S . -B build -DM3SP_ENABLE_PROFILING=ON
cmake --build build -j
cd build && ./m3sp_prof local_search      # leaves gmon.out behind
cmake --build build --target profile-report
```

## Layout

```
m3sp/
├── CMakeLists.txt      root build configuration
├── cmake/              CMake modules (CompilerWarnings.cmake)
├── docs/               reference papers
├── extern/             vendored third-party headers
│   ├── brkga/          BRKGA framework (Rodrigo Franco Toso)
│   └── mtrand/         Mersenne Twister (Richard J. Wagner)
├── include/m3sp/       public headers
│   ├── common.h        problem data, structs and globals
│   ├── decoder.h       the Solution class
│   └── decoder/        one header per decoder, plus base.h
├── src/                implementation
│   ├── main.cpp        entry point and BRKGA driver
│   ├── common.cpp
│   ├── decoder.cpp     decode() and the type dispatch
│   └── decoder/
│       ├── base.cpp    shared primitives used by every decoder
│       ├── mic.cpp     MIC construction heuristic
│       ├── dp.cpp
│       ├── gcsid.cpp
│       └── local_search.cpp
├── instances/          problem instances read at run time
└── tests/              unit tests
```

`src/decoder/base.cpp` is the shared toolbox — `insertInChannel`, `applyFusion`,
`dp` and `checkFeasibility`. Every decoder builds on it; it is not selectable as
a `type` of its own.

## Analysis scripts

`instance_generator.py` and `det_instance_generator.py` create instances;
`plot_population.py` and `plot_profile.py` chart the results of a run;
`tests/gurobi_check.py` validates solutions against an exact Gurobi model.
