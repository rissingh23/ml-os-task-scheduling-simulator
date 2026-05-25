# ML-Based OS Task Scheduling Simulator

C++20/Python simulator for comparing classic OS scheduling policies against an ML-guided scheduler.

The project is intentionally structured like a systems interview project:

- deterministic C++ simulator core
- FIFO, MLFQ, and ML-guided scheduling policies
- synthetic workload generation and CSV trace loading
- feature export for runtime-prediction models
- Python training scripts for linear and XGBoost predictors
- benchmark CSV output and correctness tests
- browser frontend with an interactive scheduler simulation and performance dashboard

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

If CMake is not installed:

```bash
make
make test
```

## Frontend

Open `frontend/index.html` in a browser.

The frontend runs a browser-side simulator for FIFO, MLFQ, and ML-guided scheduling. It lets you change workload profile, task count, arrival pressure, seed, and context-switch cost, then compares deadline-miss rate, turnaround time, waiting time, response time, context switches, and CPU timeline behavior.

## Run Benchmarks

```bash
./build/scheduler_sim --mode benchmark --profile deadline_tight --tasks 5000 --seed 42 --output results/deadline_tight.csv
```

The benchmark compares:

- `fifo`
- `mlfq`
- `ml_guided`

Metrics include average turnaround time, waiting time, response time, deadline-miss rate, CPU utilization, context switches, and makespan.

## Export Features And Train A Model

```bash
./build/scheduler_sim --mode export-features --profile deadline_tight --tasks 20000 --output results/features.csv
python3 scripts/train_linear.py --input results/features.csv --output results/runtime_linear_model.csv
./build/scheduler_sim --mode benchmark --profile deadline_tight --tasks 5000 --model results/runtime_linear_model.csv --output results/ml_guided.csv
```

For XGBoost:

```bash
python3 scripts/train_xgboost.py --input results/features.csv --output results/runtime_xgboost.json
```

The C++ simulator currently consumes the linear CSV model for online scheduling. XGBoost is included for offline runtime-prediction experiments.

## Trace Format

`--trace` accepts CSVs with this header:

```csv
arrival_time,runtime,deadline,priority,io_interval,io_duration
```

This is deliberately simple so Google/Alibaba cluster traces can be normalized into the simulator format with a small preprocessing script.

## Code Map

1. `include/sched/types.hpp`: task, metrics, and enum definitions
2. `include/sched/scheduler.hpp`: scheduler interface and policy declarations
3. `src/simulator.cpp`: discrete-time OS scheduling simulator
4. `src/schedulers/`: FIFO, MLFQ, and ML-guided policies
5. `src/workload.cpp`: synthetic workloads
6. `src/trace_loader.cpp`: CSV trace loader
7. `src/feature_export.cpp`: ML feature dataset export
8. `scripts/`: model training and result evaluation
9. `tests/test_scheduler.cpp`: smoke tests for scheduler correctness

## Resume Target

```latex
\resumeProjectHeading
{\textbf{ML-Based OS Task Scheduling Simulator} $|$ C++, Python, XGBoost, Linux, perf, Multithreading}
{}
\resumeItemListStart
\resumeItem{Built a C++ OS scheduler simulator supporting FIFO, MLFQ, and ML-guided scheduling with benchmark analysis.}
\resumeItem{Trained runtime predictors from normalized cluster-trace features and integrated a low-overhead online inference path.}
\resumeItem{Compared deadline-miss rate, turnaround time, response time, and context-switch overhead across heuristic and ML policies.}
\resumeItemListEnd
```
