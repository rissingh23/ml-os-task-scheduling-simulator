#!/usr/bin/env python3
import argparse
import csv
import json


def main():
    parser = argparse.ArgumentParser(description="Summarize scheduler benchmark CSVs.")
    parser.add_argument("--input", required=True)
    args = parser.parse_args()

    with open(args.input, newline="") as handle:
        rows = list(csv.DictReader(handle))

    baseline = next((row for row in rows if row["scheduler"] == "mlfq"), rows[0] if rows else None)
    output = {"rows": len(rows), "schedulers": []}
    for row in rows:
        miss = float(row["deadline_miss_rate"])
        base_miss = float(baseline["deadline_miss_rate"]) if baseline else 0.0
        reduction = 0.0 if base_miss == 0 else (base_miss - miss) / base_miss
        output["schedulers"].append(
            {
                "scheduler": row["scheduler"],
                "deadline_miss_rate": miss,
                "miss_reduction_vs_mlfq": reduction,
                "avg_turnaround": float(row["avg_turnaround"]),
                "context_switches": int(row["context_switches"]),
            }
        )
    print(json.dumps(output, indent=2))


if __name__ == "__main__":
    main()
