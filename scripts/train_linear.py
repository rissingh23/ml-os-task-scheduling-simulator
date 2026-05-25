#!/usr/bin/env python3
import argparse
import csv
import json
import math


FEATURE_NAMES = [
    "requested_runtime",
    "deadline_slack",
    "priority",
    "io_interval",
    "io_duration",
    "ran_since_io",
]


def load_rows(path):
    xs, ys = [], []
    with open(path, newline="") as handle:
        reader = csv.DictReader(handle)
        for row in reader:
            xs.append(
                [
                    float(row["requested_runtime"]),
                    float(row["deadline_slack"]),
                    float(row["priority"]),
                    float(row["io_interval"]),
                    float(row["io_duration"]),
                    0.0,
                ]
            )
            ys.append(float(row["label_runtime"]))
    return xs, ys


def mean(values):
    return sum(values) / max(1, len(values))


def main():
    parser = argparse.ArgumentParser(description="Train a tiny linear runtime predictor consumable by the C++ simulator.")
    parser.add_argument("--input", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--epochs", type=int, default=400)
    parser.add_argument("--lr", type=float, default=0.03)
    parser.add_argument("--ridge", type=float, default=1e-4)
    args = parser.parse_args()

    xs, ys = load_rows(args.input)
    cols = list(zip(*xs))
    means = [mean(col) for col in cols]
    scales = []
    for col, avg in zip(cols, means):
        variance = mean([(value - avg) ** 2 for value in col])
        scales.append(math.sqrt(variance) or 1.0)

    normalized = [[(value - means[i]) / scales[i] for i, value in enumerate(row)] for row in xs]
    bias = mean(ys)
    weights = [0.0 for _ in FEATURE_NAMES]

    for _ in range(args.epochs):
        grad_b = 0.0
        grad_w = [0.0 for _ in FEATURE_NAMES]
        for row, y in zip(normalized, ys):
            pred = bias + sum(weight * value for weight, value in zip(weights, row))
            error = pred - y
            grad_b += error
            for i, value in enumerate(row):
                grad_w[i] += error * value + args.ridge * weights[i]
        scale = 1.0 / max(1, len(xs))
        bias -= args.lr * grad_b * scale
        for i in range(len(weights)):
            weights[i] -= args.lr * grad_w[i] * scale

    raw_weights = [weights[i] / scales[i] for i in range(len(weights))]
    raw_bias = bias - sum(weights[i] * means[i] / scales[i] for i in range(len(weights)))

    with open(args.output, "w", newline="") as handle:
        writer = csv.writer(handle)
        writer.writerow(["bias", raw_bias])
        for index, value in enumerate(raw_weights):
            writer.writerow([index, value])

    print(json.dumps({"output": args.output, "rows": len(xs), "features": FEATURE_NAMES}, indent=2))


if __name__ == "__main__":
    main()
