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

Run the backend-powered app:

```bash
/opt/miniconda3/bin/python backend/server.py \
  --port 5175 \
  --model results/models/alibaba_xgboost_100k.json \
  --train-on-start
```

Then open:

```text
http://127.0.0.1:5175/
```

The frontend has three pages:

- `frontend/index.html`: manual task builder. Add tasks, edit A/R/D/P fields, choose FIFO, MLFQ, Round Robin, or ML-guided, and inspect the Gantt chart, metrics, and backend XGBoost batch inference.
- `frontend/simulator.html`: generated workload simulator. Change workload profile, task count, arrival pressure, seed, and context-switch cost, then compare deadline-miss rate, turnaround time, waiting time, response time, context switches, CPU timeline behavior, and per-dispatch scheduler decisions.
- `frontend/ml.html`: ML-guided scheduling explainer.

The manual page calls `/api/schedule`. For ML-guided scheduling, the backend trains/loads XGBoost, predicts every submitted task in one batch, and returns predictions plus the schedule. This intentionally allows a short loading step instead of pretending inference is free.

Current local model:

- Training source: real Alibaba `cluster-trace-v2018` `batch_task.csv`
- Sample: first 100,000 terminated batch tasks normalized into simulator task rows
- Model: `results/models/alibaba_xgboost_100k.json`
- Validation RMSE from training run: `26.71`
- Top gain features: requested runtime, deadline slack, priority

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

For XGBoost batch inference:

```bash
python3 scripts/batch_predict_xgboost.py \
  --model results/runtime_xgboost.json \
  --input results/features.csv \
  --output results/runtime_xgboost_predictions.csv \
  --batch-size 4096

./build/scheduler_sim \
  --mode benchmark \
  --profile deadline_tight \
  --tasks 5000 \
  --batch-predictions results/runtime_xgboost_predictions.csv \
  --output results/xgboost_batch_scheduler.csv
```

The scheduler hot path does not evaluate XGBoost trees directly. Python runs XGBoost in batches and writes `task_id,predicted_runtime`; C++ loads that compact CSV and does O(1) prediction lookup per ready task. This is the intended low-overhead deployment path.

On macOS, the Python `xgboost` package may require `libomp.dylib`. If import fails with an OpenMP error, install XGBoost through Conda or install `libomp` with Homebrew.

## Cluster Trace Normalization

The repo includes trace normalizers for real cluster workloads:

```bash
python3 scripts/normalize_alibaba_trace.py \
  --input path/to/batch_task.csv \
  --output data/alibaba_tasks.csv \
  --limit 100000

python3 scripts/normalize_borg_trace.py \
  --input path/to/borg_tasks.csv \
  --output data/borg_tasks.csv \
  --limit 100000
```

Then export features and train:

```bash
./build/scheduler_sim --mode export-features --trace data/alibaba_tasks.csv --output results/alibaba_features.csv
python3 scripts/train_xgboost.py --input results/alibaba_features.csv --output results/alibaba_xgboost.json
python3 scripts/batch_predict_xgboost.py --model results/alibaba_xgboost.json --input results/alibaba_features.csv --output results/alibaba_xgb_predictions.csv
./build/scheduler_sim --mode benchmark --trace data/alibaba_tasks.csv --batch-predictions results/alibaba_xgb_predictions.csv --output results/alibaba_xgb_scheduler.csv
```

Known public sources:

- Google Borg traces: `google/cluster-data`
- Alibaba cluster trace v2018: `alibaba/clusterdata/cluster-trace-v2018`

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
