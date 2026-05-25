const state = {
  tasks: [],
  nextId: 1,
};

const els = {
  algorithm: document.querySelector("#manualAlgorithm"),
  quantum: document.querySelector("#manualQuantum"),
  quantumValue: document.querySelector("#manualQuantumValue"),
  runButton: document.querySelector("#manualRunButton"),
  resetButton: document.querySelector("#manualResetButton"),
  addButton: document.querySelector("#addTaskButton"),
  table: document.querySelector("#manualTable"),
  gantt: document.querySelector("#manualGantt"),
  metrics: document.querySelector("#manualMetrics"),
  averages: document.querySelector("#manualAverages"),
  badge: document.querySelector("#manualBadge"),
  predictions: document.querySelector("#predictionList"),
  status: document.querySelector("#manualStatus"),
};

function defaultTasks() {
  return [
    { id: 1, arrival: 0, runtime: 1, deadline: 3, priority: 2 },
    { id: 2, arrival: 0, runtime: 10, deadline: 14, priority: 1 },
    { id: 3, arrival: 0, runtime: 1, deadline: 5, priority: 4 },
  ];
}

function algorithmLabel(value) {
  return {
    fifo: "FIFO",
    mlfq: "MLFQ",
    ml_guided: "ML-guided",
    rr: "Round Robin",
  }[value] || value;
}

function predictRuntime(task, now) {
  const slack = Math.max(0, task.deadline - now);
  return Math.max(1, task.runtime - task.priority * 0.2 - slack * 0.02);
}

function mlScore(task, remaining, now) {
  const slack = Math.max(0, task.deadline - now);
  return predictRuntime({ ...task, runtime: remaining }, now) + slack * 0.65 - task.priority * 2;
}

function readTasksFromDom() {
  return [...els.table.querySelectorAll(".manual-row")].map((row, index) => ({
    id: Number(row.dataset.id),
    arrival: Math.max(0, Number(row.querySelector("[data-field='arrival']").value || 0)),
    runtime: Math.max(1, Number(row.querySelector("[data-field='runtime']").value || 1)),
    deadline: Math.max(1, Number(row.querySelector("[data-field='deadline']").value || 1)),
    priority: Math.max(0, Math.min(4, Number(row.querySelector("[data-field='priority']").value || 0))),
    order: index,
  }));
}

function renderTaskRows() {
  els.table.innerHTML = `
    <div class="manual-head">
      <span>PID</span><span>A</span><span>R</span><span>D</span><span>P</span><span>Del</span>
    </div>
    ${state.tasks.map((task) => `
      <div class="manual-row" data-id="${task.id}">
        <strong>P${task.id}</strong>
        <input data-field="arrival" type="number" min="0" value="${task.arrival}" aria-label="Arrival for P${task.id}" />
        <input data-field="runtime" type="number" min="1" value="${task.runtime}" aria-label="Runtime for P${task.id}" />
        <input data-field="deadline" type="number" min="1" value="${task.deadline}" aria-label="Deadline for P${task.id}" />
        <input data-field="priority" type="number" min="0" max="4" value="${task.priority}" aria-label="Priority for P${task.id}" />
        <button class="delete-button" type="button" data-delete="${task.id}" aria-label="Delete P${task.id}">x</button>
      </div>
    `).join("")}
  `;

  els.table.querySelectorAll("input").forEach((input) => {
    input.addEventListener("input", run);
  });
  els.table.querySelectorAll("[data-delete]").forEach((button) => {
    button.addEventListener("click", () => {
      if (state.tasks.length <= 1) return;
      state.tasks = readTasksFromDom().filter((task) => task.id !== Number(button.dataset.delete));
      renderTaskRows();
      run();
    });
  });
}

function simulateManual(tasks, algorithm, quantum) {
  const items = tasks.map((task) => ({ ...task, remaining: task.runtime, level: 0, firstStart: null, finish: 0 }));
  const ready = [];
  const queues = [[], [], []];
  const quantums = [2, 4, 8];
  const segments = [];
  let now = Math.min(...items.map((task) => task.arrival));
  let current = null;
  let slice = 0;
  let finished = 0;
  let switches = 0;

  const addArrivals = () => {
    items.forEach((task, index) => {
      if (!task.added && task.arrival <= now) {
        task.added = true;
        if (algorithm === "mlfq") queues[task.level].push(index);
        else ready.push(index);
      }
    });
  };

  const pick = () => {
    if (algorithm === "fifo" || algorithm === "rr") return ready.shift();
    if (algorithm === "mlfq") {
      for (const queue of queues) {
        if (queue.length) return queue.shift();
      }
      return undefined;
    }
    if (!ready.length) return undefined;
    let bestIndex = 0;
    let bestScore = Infinity;
    ready.forEach((id, index) => {
      const score = mlScore(items[id], items[id].remaining, now);
      if (score < bestScore) {
        bestScore = score;
        bestIndex = index;
      }
    });
    return ready.splice(bestIndex, 1)[0];
  };

  const requeue = (id) => {
    if (algorithm === "mlfq") {
      items[id].level = Math.min(2, items[id].level + 1);
      queues[items[id].level].push(id);
    } else {
      ready.push(id);
    }
  };

  while (finished < items.length && now < 10000) {
    addArrivals();
    if (current === null) {
      const next = pick();
      if (next === undefined) {
        const nextArrival = Math.min(...items.filter((task) => !task.added).map((task) => task.arrival));
        now = Number.isFinite(nextArrival) ? nextArrival : now + 1;
        continue;
      }
      current = next;
      slice = 0;
      switches += 1;
      if (items[current].firstStart === null) items[current].firstStart = now;
    }

    const start = now;
    const limit = algorithm === "fifo" ? items[current].remaining
      : algorithm === "rr" ? quantum
      : algorithm === "mlfq" ? quantums[items[current].level]
      : Math.min(quantum, 16);
    const runFor = Math.min(limit - slice, items[current].remaining);
    now += runFor;
    slice += runFor;
    items[current].remaining -= runFor;
    segments.push({ taskId: items[current].id, start, end: now });
    addArrivals();

    if (items[current].remaining === 0) {
      items[current].finish = now;
      current = null;
      finished += 1;
    } else if (algorithm !== "fifo" && slice >= limit) {
      requeue(current);
      current = null;
    }
  }

  const metrics = items.map((task) => {
    const tat = task.finish - task.arrival;
    const wt = tat - task.runtime;
    const rt = task.firstStart - task.arrival;
    return { ...task, completion: task.finish, tat, wt, rt, missed: task.finish > task.deadline };
  });
  return { segments, metrics, switches, makespan: now };
}

function colorFor(id) {
  const colors = ["#b7d4f8", "#b8f3cd", "#ffe58b", "#f7b7b2", "#c8c4ff", "#b9efe9"];
  return colors[(id - 1) % colors.length];
}

function renderGantt(result) {
  const total = Math.max(1, result.makespan);
  els.gantt.innerHTML = `
    <div class="gantt-track">
      ${result.segments.map((segment) => {
        const left = (segment.start / total) * 100;
        const width = ((segment.end - segment.start) / total) * 100;
        return `<div class="gantt-block" style="left:${left}%;width:${width}%;background:${colorFor(segment.taskId)}">P${segment.taskId}</div>`;
      }).join("")}
    </div>
    <div class="gantt-ticks">
      ${result.segments.map((segment) => `<span style="left:${(segment.start / total) * 100}%">${segment.start}</span>`).join("")}
      <span style="left:100%">${result.makespan}</span>
    </div>
  `;
}

function renderMetrics(result) {
  const count = Math.max(1, result.metrics.length);
  const avgTat = result.metrics.reduce((sum, task) => sum + task.tat, 0) / count;
  const avgWt = result.metrics.reduce((sum, task) => sum + task.wt, 0) / count;
  const avgRt = result.metrics.reduce((sum, task) => sum + task.rt, 0) / count;
  els.averages.textContent = `Avg TAT ${avgTat.toFixed(2)} / WT ${avgWt.toFixed(2)} / RT ${avgRt.toFixed(2)}`;
  els.metrics.innerHTML = `
    <div class="metrics-head"><span>PID</span><span>A</span><span>R</span><span>D</span><span>CT</span><span>TAT</span><span>WT</span><span>RT</span></div>
    ${result.metrics.map((task) => `
      <div class="metrics-row ${task.missed ? "is-missed" : ""}">
        <strong>P${task.id}</strong><span>${task.arrival}</span><span>${task.runtime}</span><span>${task.deadline}</span>
        <span>${task.completion}</span><span>${task.tat}</span><span>${task.wt}</span><span>${task.rt}</span>
      </div>
    `).join("")}
  `;
}

function renderPredictions(tasks) {
  const ranked = tasks.map((task) => ({
    ...task,
    predicted: predictRuntime(task, task.arrival),
    score: mlScore(task, task.runtime, task.arrival),
  })).sort((a, b) => a.score - b.score);
  els.predictions.innerHTML = ranked.map((task) => `
    <div class="prediction-row">
      <strong>P${task.id}</strong>
      <span>pred ${task.predicted.toFixed(2)}</span>
      <span>score ${task.score.toFixed(2)}</span>
      <span>${task.score === ranked[0].score ? "first ML pick" : "candidate"}</span>
    </div>
  `).join("");
}

function run() {
  state.tasks = readTasksFromDom();
  const algorithm = els.algorithm.value;
  const quantum = Number(els.quantum.value);
  const result = simulateManual(state.tasks, algorithm, quantum);
  els.badge.textContent = `${algorithmLabel(algorithm)}${algorithm === "rr" || algorithm === "ml_guided" ? ` / q=${quantum}` : ""}`;
  renderGantt(result);
  renderMetrics(result);
  renderPredictions(state.tasks);
  els.status.textContent = "Updated";
}

function reset() {
  state.tasks = defaultTasks();
  state.nextId = 4;
  renderTaskRows();
  run();
}

els.quantum.addEventListener("input", () => {
  els.quantumValue.textContent = `${els.quantum.value} tick${Number(els.quantum.value) === 1 ? "" : "s"}`;
});
els.quantum.addEventListener("change", run);
els.algorithm.addEventListener("change", run);
els.runButton.addEventListener("click", run);
els.resetButton.addEventListener("click", reset);
els.addButton.addEventListener("click", () => {
  state.tasks = readTasksFromDom();
  const id = state.nextId;
  state.nextId += 1;
  state.tasks.push({ id, arrival: 0, runtime: 4, deadline: 10 + id, priority: 1 });
  renderTaskRows();
  run();
});

reset();
