# TaskSim Lab Project Summary

TaskSim Lab is an ML-based OS task scheduling simulator built as a resume-ready systems project. It combines a deterministic C++ scheduling core, Python/XGBoost model inference, and an interactive browser UI for inspecting how different scheduling strategies choose work.

## What It Demonstrates

- Systems design: discrete-time CPU scheduling, queues, preemption, context-switch cost, deadline tracking.
- Algorithms: FIFO, Round Robin, MLFQ, and ML-guided ranking.
- ML integration: XGBoost runtime prediction trained on normalized Alibaba cluster-trace features.
- Backend integration: Python HTTP service serving the frontend and `/api/schedule` batch inference.
- Frontend communication: manual task entry, Gantt chart, process metrics, and model score explanation.
- Deployment: Render backend service with health checks and a tracked lightweight model artifact.

## Architecture

```text
Manual task UI
      |
      v
Python backend /api/schedule
      |
      v
XGBoost batch runtime prediction
      |
      v
ML-guided scheduler ranking
      |
      v
Gantt chart + CT/TAT/WT/RT + prediction scores
```

The C++ simulator remains the benchmarkable systems core. The Python backend is the presentation layer that makes real model inference easy to demo in a browser.

## Model

The deployed model is stored at:

```text
models/alibaba_xgboost_100k.json
```

It was trained from a 100,000-row normalized sample of Alibaba `cluster-trace-v2018` batch tasks. The model predicts expected CPU runtime from:

- requested runtime
- deadline slack
- priority
- I/O interval
- I/O duration

The scheduler converts predictions into a rank score:

```text
score = predicted_runtime + 0.65 * deadline_slack - 2.0 * priority
```

Lower score runs sooner.

## Resume Bullets

```latex
\resumeProjectHeading
{\textbf{ML-Based OS Task Scheduling Simulator} $|$ C++, Python, XGBoost, Render, Multithreading}
{}
\resumeItemListStart
\resumeItem{Built a C++ OS scheduler simulator comparing FIFO, Round Robin, MLFQ, and ML-guided scheduling with deadline and turnaround metrics.}
\resumeItem{Trained an XGBoost runtime predictor on normalized Alibaba cluster-trace tasks and integrated batched backend inference for manual scheduling demos.}
\resumeItem{Deployed an interactive browser UI with Gantt charts, process metrics, and model-score explanations for inspecting scheduling decisions end to end.}
\resumeItemListEnd
```
