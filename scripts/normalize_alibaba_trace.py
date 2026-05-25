#!/usr/bin/env python3
import argparse
import csv
import json
import math


def first_present(row, names, default=""):
    for name in names:
        if name in row and row[name] not in ("", "NA", "null"):
            return row[name]
    return default


def to_float(value, default=0.0):
    try:
        return float(value)
    except Exception:
        return default


def main():
    parser = argparse.ArgumentParser(description="Normalize Alibaba batch_task-style traces into scheduler_sim task CSV.")
    parser.add_argument("--input", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--limit", type=int, default=0)
    parser.add_argument("--time-scale", type=float, default=60.0, help="Divide raw seconds by this value to reduce simulator ticks.")
    args = parser.parse_args()

    written = 0
    min_start = None
    with open(args.input, newline="") as source, open(args.output, "w", newline="") as dest:
        reader = csv.DictReader(source)
        writer = csv.writer(dest)
        writer.writerow(["arrival_time", "runtime", "deadline", "priority", "io_interval", "io_duration"])
        for row in reader:
            start = to_float(first_present(row, ["start_time", "start_timestamp", "start", "ts"], "0"))
            end = to_float(first_present(row, ["end_time", "end_timestamp", "end"], str(start + 1)))
            if min_start is None:
                min_start = start
            runtime_raw = max(1.0, end - start)
            arrival = max(0, int((start - min_start) / args.time_scale))
            runtime = max(1, int(math.ceil(runtime_raw / args.time_scale)))
            instance_count = to_float(first_present(row, ["instance_num", "instance_count", "num_instances"], "1"), 1.0)
            priority = max(0, min(4, int(math.log2(max(1.0, instance_count)))))
            deadline = arrival + runtime + max(4, runtime * 2)
            writer.writerow([arrival, runtime, deadline, priority, 0, 0])
            written += 1
            if args.limit and written >= args.limit:
                break

    print(json.dumps({"input": args.input, "output": args.output, "rows": written}, indent=2))


if __name__ == "__main__":
    main()
