#!/usr/bin/env python3
import argparse
import json
import math
import os
import random
import time
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlparse

try:
    import xgboost as xgb
except Exception as exc:
    xgb = None
    XGBOOST_IMPORT_ERROR = str(exc)
else:
    XGBOOST_IMPORT_ERROR = ""


ROOT = Path(__file__).resolve().parents[1]
FRONTEND = ROOT / "frontend"
FEATURE_NAMES = ["requested_runtime", "deadline_slack", "priority", "io_interval", "io_duration"]


class ModelStore:
    def __init__(self, model_path=None):
        self.booster = None
        self.metrics = {}
        self.trained_at = None
        self.model_path = Path(model_path) if model_path else None

    def ensure_trained(self):
        if self.booster is not None:
            return
        if xgb is None:
            raise RuntimeError(f"xgboost unavailable: {XGBOOST_IMPORT_ERROR}")
        if self.model_path:
            if not self.model_path.exists():
                raise RuntimeError(f"XGBoost model file not found: {self.model_path}")
            self.booster = xgb.Booster()
            self.booster.load_model(str(self.model_path))
            self.metrics = {
                "source": str(self.model_path),
                "mode": "loaded_real_trace_model",
                "importance": self.booster.get_score(importance_type="gain"),
            }
            self.trained_at = time.time()
            return

        rng = random.Random(42)
        xs, ys = [], []
        for _ in range(14000):
            requested = rng.randint(1, 140)
            slack = rng.randint(0, 220)
            priority = rng.randint(0, 4)
            io_interval = rng.choice([0, 0, 0, rng.randint(2, 40)])
            io_duration = 0 if io_interval == 0 else rng.randint(1, 12)
            noise = rng.gauss(0, max(0.8, requested * 0.04))
            runtime = max(1.0, requested + (0.18 * io_duration) - (0.08 * priority) + noise)
            xs.append([requested, slack, priority, io_interval, io_duration])
            ys.append(runtime)

        split = int(len(xs) * 0.82)
        dtrain = xgb.DMatrix(xs[:split], label=ys[:split], feature_names=FEATURE_NAMES)
        dtest = xgb.DMatrix(xs[split:], label=ys[split:], feature_names=FEATURE_NAMES)
        params = {
            "objective": "reg:squarederror",
            "eval_metric": "rmse",
            "max_depth": 4,
            "eta": 0.08,
            "subsample": 0.9,
            "colsample_bytree": 0.9,
            "seed": 42,
        }
        start = time.perf_counter()
        self.booster = xgb.train(params, dtrain, num_boost_round=90, evals=[(dtest, "test")], verbose_eval=False)
        predictions = self.booster.predict(dtest)
        rmse = math.sqrt(sum((float(pred) - float(label)) ** 2 for pred, label in zip(predictions, ys[split:])) / max(1, len(predictions)))
        self.metrics = {
            "rows": len(xs),
            "test_rows": len(xs) - split,
            "rmse": rmse,
            "train_ms": (time.perf_counter() - start) * 1000.0,
            "importance": self.booster.get_score(importance_type="gain"),
        }
        self.trained_at = time.time()

    def predict(self, tasks):
        self.ensure_trained()
        xs = []
        for task in tasks:
            slack = max(0, task["deadline"] - task["arrival"])
            xs.append([task["runtime"], slack, task["priority"], task.get("io_interval", 0), task.get("io_duration", 0)])
        matrix = xgb.DMatrix(xs, feature_names=FEATURE_NAMES)
        raw = self.booster.predict(matrix)
        return {task["id"]: max(1.0, float(prediction)) for task, prediction in zip(tasks, raw)}

MODEL = ModelStore()


def score_task(task, remaining, now, predictions):
    slack = max(0, task["deadline"] - now)
    predicted = max(1.0, predictions.get(task["id"], remaining))
    return predicted + 0.65 * slack - 2.0 * task["priority"]


def simulate(tasks, algorithm, quantum, predictions):
    items = [{**task, "remaining": task["runtime"], "level": 0, "first_start": None, "finish": 0} for task in tasks]
    items.sort(key=lambda item: (item["arrival"], item["id"]))
    ready = []
    queues = [[], [], []]
    quantums = [2, 4, 8]
    segments = []
    decisions = []
    now = min((task["arrival"] for task in items), default=0)
    current = None
    slice_used = 0
    finished = 0

    def add_arrivals():
        for index, task in enumerate(items):
            if not task.get("added") and task["arrival"] <= now:
                task["added"] = True
                if algorithm == "mlfq":
                    queues[task["level"]].append(index)
                else:
                    ready.append(index)

    def candidate_ids():
        if algorithm == "mlfq":
            return [idx for queue in queues for idx in queue]
        return list(ready)

    def pick():
        candidates = candidate_ids()
        if algorithm in ("fifo", "rr"):
            chosen = ready.pop(0) if ready else None
        elif algorithm == "mlfq":
            chosen = None
            for queue in queues:
                if queue:
                    chosen = queue.pop(0)
                    break
        else:
            if not ready:
                chosen = None
            else:
                best_pos = min(range(len(ready)), key=lambda pos: score_task(items[ready[pos]], items[ready[pos]]["remaining"], now, predictions))
                chosen = ready.pop(best_pos)
        if chosen is not None:
            decisions.append({
                "time": now,
                "chosen": items[chosen]["id"],
                "candidates": [
                    {
                        "id": items[idx]["id"],
                        "remaining": items[idx]["remaining"],
                        "predicted": predictions.get(items[idx]["id"], items[idx]["remaining"]),
                        "slack": max(0, items[idx]["deadline"] - now),
                        "priority": items[idx]["priority"],
                        "level": items[idx]["level"],
                        "score": score_task(items[idx], items[idx]["remaining"], now, predictions),
                    }
                    for idx in candidates
                ],
            })
        return chosen

    def requeue(index):
        if algorithm == "mlfq":
            items[index]["level"] = min(2, items[index]["level"] + 1)
            queues[items[index]["level"]].append(index)
        else:
            ready.append(index)

    while finished < len(items) and now < 100000:
        add_arrivals()
        if current is None:
            next_index = pick()
            if next_index is None:
                future = [task["arrival"] for task in items if not task.get("added")]
                now = min(future) if future else now + 1
                continue
            current = next_index
            slice_used = 0
            if items[current]["first_start"] is None:
                items[current]["first_start"] = now

        limit = items[current]["remaining"] if algorithm == "fifo" else quantum if algorithm in ("rr", "ml_guided") else quantums[items[current]["level"]]
        run_for = min(limit - slice_used, items[current]["remaining"])
        start = now
        now += run_for
        slice_used += run_for
        items[current]["remaining"] -= run_for
        segments.append({"taskId": items[current]["id"], "start": start, "end": now})
        add_arrivals()

        if items[current]["remaining"] == 0:
            items[current]["finish"] = now
            current = None
            finished += 1
        elif algorithm != "fifo" and slice_used >= limit:
            requeue(current)
            current = None

    metrics = []
    for task in sorted(items, key=lambda item: item["id"]):
        tat = task["finish"] - task["arrival"]
        wt = tat - task["runtime"]
        rt = (task["first_start"] or task["arrival"]) - task["arrival"]
        metrics.append({
            "id": task["id"],
            "arrival": task["arrival"],
            "runtime": task["runtime"],
            "deadline": task["deadline"],
            "priority": task["priority"],
            "completion": task["finish"],
            "tat": tat,
            "wt": wt,
            "rt": rt,
            "missed": task["finish"] > task["deadline"],
            "predicted": predictions.get(task["id"], task["runtime"]),
        })

    count = max(1, len(metrics))
    return {
        "segments": segments,
        "metrics": metrics,
        "decisions": decisions,
        "makespan": now,
        "averages": {
            "tat": sum(row["tat"] for row in metrics) / count,
            "wt": sum(row["wt"] for row in metrics) / count,
            "rt": sum(row["rt"] for row in metrics) / count,
            "missRate": sum(1 for row in metrics if row["missed"]) / count,
        },
    }


class Handler(SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=str(FRONTEND), **kwargs)

    def _json(self, status, payload):
        body = json.dumps(payload).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        path = urlparse(self.path).path
        if path == "/api/health":
            self._json(200, {
                "ok": True,
                "service": "tasksim-lab",
                "model_loaded": MODEL.booster is not None,
                "model_source": str(MODEL.model_path) if MODEL.model_path else "startup_training",
            })
            return
        if path == "/api/model":
            try:
                MODEL.ensure_trained()
                self._json(200, {"ok": True, "model": MODEL.metrics})
            except Exception as exc:
                self._json(503, {"ok": False, "error": str(exc)})
            return
        super().do_GET()

    def do_POST(self):
        parsed = urlparse(self.path)
        length = int(self.headers.get("Content-Length", "0"))
        payload = json.loads(self.rfile.read(length) or b"{}")
        if parsed.path != "/api/schedule":
            self._json(404, {"ok": False, "error": "unknown endpoint"})
            return
        try:
            tasks = payload["tasks"]
            algorithm = payload.get("algorithm", "ml_guided")
            quantum = int(payload.get("quantum", 2))
            predictions = MODEL.predict(tasks) if algorithm == "ml_guided" else {task["id"]: task["runtime"] for task in tasks}
            result = simulate(tasks, algorithm, quantum, predictions)
            result["model"] = MODEL.metrics if algorithm == "ml_guided" else {"kind": "not_used"}
            result["algorithm"] = algorithm
            self._json(200, {"ok": True, **result})
        except Exception as exc:
            self._json(500, {"ok": False, "error": str(exc)})


def main():
    parser = argparse.ArgumentParser(description="TaskSim Lab backend and static frontend server.")
    parser.add_argument("--host", default=os.environ.get("HOST", "127.0.0.1"))
    parser.add_argument("--port", type=int, default=int(os.environ.get("PORT", "5175")))
    parser.add_argument("--model", default=os.environ.get("MODEL_PATH", ""), help="Optional trained XGBoost model JSON to load instead of startup training.")
    parser.add_argument("--train-on-start", action="store_true")
    args = parser.parse_args()
    global MODEL
    MODEL = ModelStore(args.model or None)
    if args.train_on_start:
        MODEL.ensure_trained()
    server = ThreadingHTTPServer((args.host, args.port), Handler)
    print(f"TaskSim Lab running at http://{args.host}:{args.port}/")
    server.serve_forever()


if __name__ == "__main__":
    main()
