#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build"
SIM_BIN="${BUILD_DIR}/scheduler_sim"
FRONTEND_RESULTS_DIR="${ROOT_DIR}/frontend/results"

mkdir -p "${BUILD_DIR}" "${ROOT_DIR}/results"
rm -rf "${FRONTEND_RESULTS_DIR}"
mkdir -p "${FRONTEND_RESULTS_DIR}"

if [[ ! -x "${SIM_BIN}" ]]; then
  echo "building ${SIM_BIN}"
  c++ -std=c++20 -O3 -pthread -I"${ROOT_DIR}/include" \
    "${ROOT_DIR}/src/benchmark.cpp" \
    "${ROOT_DIR}/src/feature_export.cpp" \
    "${ROOT_DIR}/src/main.cpp" \
    "${ROOT_DIR}/src/ml_model.cpp" \
    "${ROOT_DIR}/src/schedulers/fifo_scheduler.cpp" \
    "${ROOT_DIR}/src/schedulers/ml_scheduler.cpp" \
    "${ROOT_DIR}/src/schedulers/mlfq_scheduler.cpp" \
    "${ROOT_DIR}/src/simulator.cpp" \
    "${ROOT_DIR}/src/trace_loader.cpp" \
    "${ROOT_DIR}/src/types.cpp" \
    "${ROOT_DIR}/src/workload.cpp" \
    -o "${SIM_BIN}"
fi

"${SIM_BIN}" --mode benchmark --profile balanced --tasks 3000 --seed 42 --output "${ROOT_DIR}/results/balanced.csv"
"${SIM_BIN}" --mode benchmark --profile deadline_tight --tasks 3000 --seed 42 --output "${ROOT_DIR}/results/deadline_tight.csv"
"${SIM_BIN}" --mode benchmark --profile io_heavy --tasks 3000 --seed 42 --output "${ROOT_DIR}/results/io_heavy.csv"
"${SIM_BIN}" --mode benchmark --profile bursty --tasks 3000 --seed 42 --output "${ROOT_DIR}/results/bursty.csv"

cp "${ROOT_DIR}/results/balanced.csv" "${FRONTEND_RESULTS_DIR}/"
cp "${ROOT_DIR}/results/deadline_tight.csv" "${FRONTEND_RESULTS_DIR}/"
cp "${ROOT_DIR}/results/io_heavy.csv" "${FRONTEND_RESULTS_DIR}/"
cp "${ROOT_DIR}/results/bursty.csv" "${FRONTEND_RESULTS_DIR}/"

cat > "${FRONTEND_RESULTS_DIR}/deploy_info.json" <<'JSON'
{
  "mode": "static",
  "note": "Static deployment serves the interactive frontend. Run the Python backend locally for live XGBoost /api/schedule inference."
}
JSON

echo "frontend bundle prepared at ${FRONTEND_RESULTS_DIR}"
