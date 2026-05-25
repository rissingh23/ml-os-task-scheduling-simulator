#!/usr/bin/env python3
import argparse
import csv
import json
import math
import sys

try:
    import xgboost as xgb
except Exception as exc:
    xgb = None
    XGBOOST_IMPORT_ERROR = exc
else:
    XGBOOST_IMPORT_ERROR = None


FEATURE_NAMES = [
    "requested_runtime",
    "deadline_slack",
    "priority",
    "io_interval",
    "io_duration",
]


def batches(rows, size):
    for i in range(0, len(rows), size):
        yield rows[i:i + size]


def load_rows(path):
    with open(path, newline="") as handle:
        reader = csv.DictReader(handle)
        rows = list(reader)
    return rows


def main():
    parser = argparse.ArgumentParser(description="Run batched XGBoost runtime inference and write task_id,predicted_runtime CSV.")
    parser.add_argument("--model", required=True)
    parser.add_argument("--input", required=True, help="Feature CSV from scheduler_sim --mode export-features.")
    parser.add_argument("--output", required=True)
    parser.add_argument("--batch-size", type=int, default=4096)
    args = parser.parse_args()

    if xgb is None:
        print(f"xgboost is unavailable: {XGBOOST_IMPORT_ERROR}", file=sys.stderr)
        print("On macOS, xgboost may also require libomp.dylib from Homebrew or Conda.", file=sys.stderr)
        raise SystemExit(1)

    rows = load_rows(args.input)
    booster = xgb.Booster()
    booster.load_model(args.model)

    prediction_count = 0
    with open(args.output, "w", newline="") as handle:
        writer = csv.writer(handle)
        writer.writerow(["task_id", "predicted_runtime"])
        for chunk in batches(rows, max(1, args.batch_size)):
            xs = [[float(row[name]) for name in FEATURE_NAMES] for row in chunk]
            matrix = xgb.DMatrix(xs, feature_names=FEATURE_NAMES)
            predictions = booster.predict(matrix)
            for row, prediction in zip(chunk, predictions):
                writer.writerow([row["task_id"], max(1.0, float(prediction))])
                prediction_count += 1

    print(json.dumps({
        "model": args.model,
        "input": args.input,
        "output": args.output,
        "batch_size": args.batch_size,
        "predictions": prediction_count,
    }, indent=2))


if __name__ == "__main__":
    main()
