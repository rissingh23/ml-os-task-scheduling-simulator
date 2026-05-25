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


def load_rows(path):
    xs, ys = [], []
    with open(path, newline="") as handle:
        reader = csv.DictReader(handle)
        for row in reader:
            xs.append([float(row[name]) for name in FEATURE_NAMES])
            ys.append(float(row["label_runtime"]))
    return xs, ys


def main():
    parser = argparse.ArgumentParser(description="Train XGBoost runtime predictor from exported scheduler task features.")
    parser.add_argument("--input", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--rounds", type=int, default=80)
    args = parser.parse_args()

    if xgb is None:
        print(f"xgboost is unavailable: {XGBOOST_IMPORT_ERROR}", file=sys.stderr)
        print("On macOS, xgboost may also require libomp.dylib from Homebrew or Conda.", file=sys.stderr)
        raise SystemExit(1)

    xs, ys = load_rows(args.input)
    split = max(1, int(len(xs) * 0.8))
    dtrain = xgb.DMatrix(xs[:split], label=ys[:split], feature_names=FEATURE_NAMES)
    dtest = xgb.DMatrix(xs[split:], label=ys[split:], feature_names=FEATURE_NAMES)
    params = {
        "objective": "reg:squarederror",
        "eval_metric": "rmse",
        "max_depth": 4,
        "eta": 0.08,
        "subsample": 0.85,
        "colsample_bytree": 0.85,
    }
    booster = xgb.train(params, dtrain, num_boost_round=args.rounds, evals=[(dtest, "test")], verbose_eval=False)
    predictions = booster.predict(dtest)
    rmse = math.sqrt(sum((float(pred) - float(label)) ** 2 for pred, label in zip(predictions, ys[split:])) / max(1, len(predictions)))
    booster.save_model(args.output)
    importance = booster.get_score(importance_type="gain")
    print(json.dumps({
        "output": args.output,
        "rows": len(xs),
        "test_rows": len(xs) - split,
        "rmse": rmse,
        "feature_importance_gain": importance,
    }, indent=2))


if __name__ == "__main__":
    main()
