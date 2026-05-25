#!/usr/bin/env python3
import argparse
import csv
import json
import sys

try:
    import xgboost as xgb
except Exception:
    xgb = None


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
        print("xgboost is not installed. Install it or use scripts/train_linear.py first.", file=sys.stderr)
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
    booster.save_model(args.output)
    print(json.dumps({"output": args.output, "rows": len(xs), "test_rows": len(xs) - split}, indent=2))


if __name__ == "__main__":
    main()
