const state = {
  metric: "missRate",
  results: [],
  tasks: [],
};

const els = {
  profile: document.querySelector("#profile"),
  taskCount: document.querySelector("#taskCount"),
  taskCountValue: document.querySelector("#taskCountValue"),
  load: document.querySelector("#load"),
  loadValue: document.querySelector("#loadValue"),
  seed: document.querySelector("#seed"),
  contextSwitch: document.querySelector("#contextSwitch"),
  contextSwitchValue: document.querySelector("#contextSwitchValue"),
  runButton: document.querySelector("#runButton"),
  randomizeButton: document.querySelector("#randomizeButton"),
  runStatus: document.querySelector("#runStatus"),
  metricStrip: document.querySelector("#metricStrip"),
  schedulerTable: document.querySelector("#schedulerTable"),
  bestScheduler: document.querySelector("#bestScheduler"),
  barChart: document.querySelector("#barChart"),
  timeline: document.querySelector("#timeline"),
  timelineScheduler: document.querySelector("#timelineScheduler"),
  selectedTask: document.querySelector("#selectedTask"),
  taskGrid: document.querySelector("#taskGrid"),
};

function mulberry32(seed) {
  let value = seed >>> 0;
  return () => {
    value += 0x6D2B79F5;
    let next = value;
    next = Math.imul(next ^ (next >>> 15), next | 1);
    next ^= next + Math.imul(next ^ (next >>> 7), next | 61);
    return ((next ^ (next >>> 14)) >>> 0) / 4294967296;
  };
}

function intBetween(rng, min, max) {
  return Math.floor(rng() * (max - min + 1)) + min;
}

function generateTasks(profile, count, seed, loadPct) {
  const rng = mulberry32(seed);
  const pressure = loadPct / 100;
  let now = 0;
  const tasks = [];

  for (let i = 0; i < count; i += 1) {
    let runtime = 0;
    let gap = 0;
    let slack = 0;
    let ioInterval = 0;
    let ioDuration = 0;

    if (profile === "balanced") {
      runtime = intBetween(rng, 4, 80);
      gap = Math.max(1, Math.round(intBetween(rng, 8, 72) / pressure));
      slack = runtime + 40 + intBetween(rng, 4, 80);
    } else if (profile === "deadline_tight") {
      runtime = intBetween(rng, 4, 80);
      gap = Math.max(1, Math.round(intBetween(rng, 6, 48) / pressure));
      slack = runtime + 10 + intBetween(rng, 2, 24);
    } else if (profile === "io_heavy") {
      runtime = intBetween(rng, 4, 80);
      gap = Math.max(1, Math.round(intBetween(rng, 8, 72) / pressure));
      ioInterval = Math.max(2, Math.floor(runtime / 4));
      ioDuration = intBetween(rng, 2, 9);
      slack = runtime + ioDuration * 3 + intBetween(rng, 2, 24);
    } else {
      runtime = rng() < 0.1 ? intBetween(rng, 40, 160) : intBetween(rng, 2, 24);
      gap = i % 40 === 0 ? Math.round(120 / pressure) : intBetween(rng, 1, 2);
      slack = runtime + 30 + intBetween(rng, 4, 80);
    }

    now += gap;
    tasks.push({
      id: i,
      arrival: now,
      runtime,
      remaining: runtime,
      deadline: now + slack,
      priority: intBetween(rng, 0, 4),
      ioInterval,
      ioDuration,
      ranSinceIo: 0,
    });
  }
  return tasks;
}

function cloneTasks(tasks) {
  return tasks.map((task) => ({ ...task }));
}

function predictor(task, now) {
  const slack = Math.max(0, task.deadline - now);
  return Math.max(1, task.remaining - task.priority * 0.15 + task.ioDuration * 0.05 - slack * 0.01);
}

function simulate(kind, sourceTasks, contextSwitchCost) {
  const tasks = cloneTasks(sourceTasks).sort((a, b) => a.arrival - b.arrival).map((task, index) => ({ ...task, id: index }));
  const meta = tasks.map(() => ({
    blocked: false,
    unblock: 0,
    started: false,
    firstStart: 0,
    finish: 0,
    preemptions: 0,
    level: 0,
  }));
  const ready = [];
  const queues = [[], [], []];
  const quantums = [2, 4, 8];
  const segments = [];
  let now = tasks[0]?.arrival ?? 0;
  let nextArrival = 0;
  let finished = 0;
  let running = null;
  let slice = 0;
  let busy = 0;
  let switches = 0;

  const pushReady = (id) => {
    if (kind === "mlfq") {
      queues[Math.min(meta[id].level, queues.length - 1)].push(id);
    } else {
      ready.push(id);
    }
  };

  const pick = () => {
    if (kind === "fifo") return ready.shift();
    if (kind === "mlfq") {
      for (const queue of queues) {
        if (queue.length) return queue.shift();
      }
      return undefined;
    }
    if (!ready.length) return undefined;
    let bestIndex = 0;
    let bestScore = Infinity;
    for (let i = 0; i < ready.length; i += 1) {
      const task = tasks[ready[i]];
      const slack = Math.max(0, task.deadline - now);
      const score = predictor(task, now) + slack * 0.65 - task.priority * 2;
      if (score < bestScore) {
        bestScore = score;
        bestIndex = i;
      }
    }
    return ready.splice(bestIndex, 1)[0];
  };

  const nextEvent = () => {
    let next = nextArrival < tasks.length ? tasks[nextArrival].arrival : Infinity;
    for (const item of meta) {
      if (item.blocked) next = Math.min(next, item.unblock);
    }
    return next;
  };

  while (finished < tasks.length && now < 1_000_000) {
    while (nextArrival < tasks.length && tasks[nextArrival].arrival <= now) {
      pushReady(nextArrival);
      nextArrival += 1;
    }
    for (let i = 0; i < tasks.length; i += 1) {
      if (meta[i].blocked && meta[i].unblock <= now) {
        meta[i].blocked = false;
        if (kind === "mlfq") meta[i].level = 0;
        pushReady(i);
      }
    }

    if (running === null) {
      const next = pick();
      if (next !== undefined) {
        running = next;
        slice = 0;
        switches += 1;
        now += contextSwitchCost;
      }
    }

    if (running === null) {
      const next = nextEvent();
      if (next === Infinity) break;
      now = Math.max(now + 1, next);
      continue;
    }

    const task = tasks[running];
    const item = meta[running];
    if (!item.started) {
      item.started = true;
      item.firstStart = now;
    }

    const start = now;
    task.remaining -= 1;
    task.ranSinceIo += 1;
    slice += 1;
    busy += 1;
    now += 1;
    segments.push({ taskId: task.id, start, end: now, deadline: task.deadline, scheduler: kind });

    if (task.remaining === 0) {
      item.finish = now;
      running = null;
      finished += 1;
      continue;
    }

    if (task.ioInterval > 0 && task.ranSinceIo >= task.ioInterval) {
      task.ranSinceIo = 0;
      item.blocked = true;
      item.unblock = now + task.ioDuration;
      running = null;
      continue;
    }

    const shouldPreempt = kind === "mlfq"
      ? slice >= quantums[Math.min(item.level, quantums.length - 1)]
      : kind === "ml_guided" && slice >= 16;

    if (shouldPreempt) {
      item.preemptions += 1;
      if (kind === "mlfq") item.level = Math.min(item.level + 1, quantums.length - 1);
      pushReady(running);
      running = null;
      slice = 0;
    }
  }

  let missed = 0;
  let turnaround = 0;
  let waiting = 0;
  let response = 0;
  const taskResults = tasks.map((task, i) => {
    const item = meta[i];
    const turn = item.finish - task.arrival;
    const wait = Math.max(0, turn - task.runtime);
    const resp = Math.max(0, item.firstStart - task.arrival);
    const missedDeadline = item.finish > task.deadline;
    missed += missedDeadline ? 1 : 0;
    turnaround += turn;
    waiting += wait;
    response += resp;
    return { ...task, start: item.firstStart, finish: item.finish, preemptions: item.preemptions, missed: missedDeadline };
  });

  const count = tasks.length || 1;
  return {
    name: kind,
    label: kind === "ml_guided" ? "ML-guided" : kind.toUpperCase(),
    missRate: missed / count,
    turnaround: turnaround / count,
    waiting: waiting / count,
    response: response / count,
    switches,
    utilization: now ? busy / now : 0,
    makespan: now,
    segments,
    tasks: taskResults,
  };
}

function formatPct(value) {
  return `${(value * 100).toFixed(1)}%`;
}

function formatNum(value) {
  return value >= 100 ? value.toFixed(0) : value.toFixed(1);
}

function renderMetrics(best, baseline) {
  const reduction = baseline ? Math.max(0, (baseline.missRate - best.missRate) / Math.max(0.0001, baseline.missRate)) : 0;
  const metrics = [
    ["Best miss rate", formatPct(best.missRate), best.label],
    ["Miss reduction", formatPct(reduction), "vs MLFQ"],
    ["Avg turnaround", formatNum(best.turnaround), "ticks"],
    ["Context switches", best.switches.toLocaleString(), best.label],
  ];
  els.metricStrip.innerHTML = metrics.map(([label, value, note]) => `
    <div class="metric">
      <span>${label}</span>
      <strong>${value}</strong>
      <em>${note}</em>
    </div>
  `).join("");
}

function renderTable(results) {
  els.schedulerTable.innerHTML = results.map((result) => `
    <div class="scheduler-row">
      <div class="scheduler-name">${result.label}</div>
      <div class="cell"><span>Miss</span><strong>${formatPct(result.missRate)}</strong></div>
      <div class="cell"><span>Turn</span><strong>${formatNum(result.turnaround)}</strong></div>
      <div class="cell"><span>Wait</span><strong>${formatNum(result.waiting)}</strong></div>
      <div class="cell"><span>Switch</span><strong>${result.switches.toLocaleString()}</strong></div>
    </div>
  `).join("");
}

function renderChart(results) {
  const metric = state.metric;
  const max = Math.max(...results.map((item) => item[metric]), 0.001);
  els.barChart.innerHTML = results.map((result, index) => {
    const value = result[metric];
    const width = `${Math.max(3, (value / max) * 100)}%`;
    const display = metric === "missRate" || metric === "utilization" ? formatPct(value) : formatNum(value);
    const tone = index === 0 ? "cool" : metric === "missRate" ? "warn" : "";
    return `
      <div class="bar-row">
        <div class="bar-label">${result.label}</div>
        <div class="bar-track"><div class="bar-fill ${tone}" style="width:${width}"></div></div>
        <div class="bar-value">${display}</div>
      </div>
    `;
  }).join("");
}

function coalesceSegments(segments) {
  const merged = [];
  for (const segment of segments) {
    const last = merged[merged.length - 1];
    if (last && last.taskId === segment.taskId && last.end === segment.start) {
      last.end = segment.end;
    } else {
      merged.push({ ...segment });
    }
  }
  return merged;
}

function renderTimeline(result) {
  const makespan = Math.max(1, result.makespan);
  const chunks = coalesceSegments(result.segments).slice(0, 260);
  els.timeline.innerHTML = `
    <div class="lane">
      <div class="lane-label"><span>${result.label} CPU</span><span>${result.makespan} ticks</span></div>
      <div class="lane-track">
        ${chunks.map((segment) => {
          const left = (segment.start / makespan) * 100;
          const width = Math.max(0.18, ((segment.end - segment.start) / makespan) * 100);
          return `<div class="segment-block" data-task="${segment.taskId}" data-start="${segment.start}" data-end="${segment.end}" style="left:${left}%;width:${width}%"></div>`;
        }).join("")}
      </div>
    </div>
  `;

  els.timeline.querySelectorAll(".segment-block").forEach((block) => {
    block.addEventListener("mouseenter", () => {
      const task = result.tasks[Number(block.dataset.task)];
      els.selectedTask.innerHTML = `
        <span>Selected task</span>
        <strong>T${task.id} ran ${block.dataset.start}-${block.dataset.end}; arrival ${task.arrival}, deadline ${task.deadline}, finish ${task.finish}, ${task.missed ? "missed" : "met"}.</strong>
      `;
    });
  });
}

function renderTaskGrid(tasks) {
  els.taskGrid.innerHTML = tasks.slice(0, 12).map((task) => `
    <div class="task-tile">
      <strong>T${task.id}</strong>
      <span>a ${task.arrival} · r ${task.runtime}</span>
      <span>d ${task.deadline} · p ${task.priority}</span>
    </div>
  `).join("");
}

function run() {
  els.runStatus.textContent = "Running";
  const profile = els.profile.value;
  const count = Number(els.taskCount.value);
  const seed = Number(els.seed.value);
  const load = Number(els.load.value);
  const contextSwitch = Number(els.contextSwitch.value);

  state.tasks = generateTasks(profile, count, seed, load);
  const results = ["fifo", "mlfq", "ml_guided"].map((kind) => simulate(kind, state.tasks, contextSwitch));
  state.results = results;

  const best = [...results].sort((a, b) => a.missRate - b.missRate || a.turnaround - b.turnaround)[0];
  const baseline = results.find((result) => result.name === "mlfq");
  els.bestScheduler.textContent = `Best: ${best.label}`;
  renderMetrics(best, baseline);
  renderTable(results);
  renderChart(results);

  els.timelineScheduler.innerHTML = results.map((result) => `<option value="${result.name}">${result.label}</option>`).join("");
  els.timelineScheduler.value = best.name;
  renderTimeline(best);
  renderTaskGrid(state.tasks);
  els.runStatus.textContent = "Updated";
}

function syncOutputs() {
  els.taskCountValue.textContent = els.taskCount.value;
  els.loadValue.textContent = `${els.load.value}%`;
  els.contextSwitchValue.textContent = `${els.contextSwitch.value} tick${Number(els.contextSwitch.value) === 1 ? "" : "s"}`;
}

els.runButton.addEventListener("click", run);
els.randomizeButton.addEventListener("click", () => {
  els.seed.value = String(Math.floor(Math.random() * 9998) + 1);
  run();
});
els.timelineScheduler.addEventListener("change", () => {
  const result = state.results.find((item) => item.name === els.timelineScheduler.value);
  if (result) renderTimeline(result);
});
document.querySelectorAll(".segment").forEach((button) => {
  button.addEventListener("click", () => {
    document.querySelectorAll(".segment").forEach((item) => item.classList.remove("is-active"));
    button.classList.add("is-active");
    state.metric = button.dataset.metric;
    renderChart(state.results);
  });
});
[els.taskCount, els.load, els.contextSwitch].forEach((input) => {
  input.addEventListener("input", syncOutputs);
  input.addEventListener("change", run);
});
[els.profile, els.seed].forEach((input) => input.addEventListener("change", run));

syncOutputs();
run();
