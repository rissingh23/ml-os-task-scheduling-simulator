# Deployment

TaskSim Lab supports two deployment modes.

## Render Backend Deployment

Use the checked-in `render.yaml` blueprint. It creates a Python web service that serves the frontend and exposes:

- `/`
- `/simulator.html`
- `/ml.html`
- `/api/health`
- `/api/model`
- `/api/schedule`

Render settings are already encoded:

```text
Runtime: Python
Build command: pip install -r requirements.txt
Start command: python backend/server.py --host 0.0.0.0 --model models/alibaba_xgboost_100k.json --train-on-start
Health check: /api/health
```

The backend reads Render's `PORT` environment variable automatically.

## Static Deployment

The `vercel.json` path matches the order-book project. It publishes `frontend/` after running:

```bash
bash scripts/prepare_frontend_bundle.sh
```

Static deployment is useful for the UI and simulator pages, but it cannot run real XGBoost `/api/schedule` inference unless paired with a backend service.

## Local Production Check

```bash
python backend/server.py \
  --host 0.0.0.0 \
  --port 5175 \
  --model models/alibaba_xgboost_100k.json \
  --train-on-start
```

Then check:

```text
http://127.0.0.1:5175/api/health
```
