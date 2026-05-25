#include "sched/simulator.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>

namespace sched {
namespace {

struct RuntimeBookkeeping {
  bool arrived{false};
  bool finished{false};
  bool blocked{false};
  Time unblock_time{};
  Time first_start_time{};
  bool has_started{false};
  Time finish_time{};
  Time waiting_time{};
  std::uint64_t preemptions{};
};

Time next_event_time(const std::vector<Task>& tasks, const std::vector<RuntimeBookkeeping>& state, std::size_t next_arrival) {
  Time next = std::numeric_limits<Time>::max();
  if (next_arrival < tasks.size()) {
    next = std::min(next, tasks[next_arrival].arrival_time);
  }
  for (const auto& item : state) {
    if (item.blocked) {
      next = std::min(next, item.unblock_time);
    }
  }
  return next;
}

}  // namespace

SimulationOutput run_simulation(
    IScheduler& scheduler,
    std::string input_label,
    const std::vector<Task>& input_tasks,
    SimulationConfig config) {
  std::vector<Task> tasks = input_tasks;
  std::sort(tasks.begin(), tasks.end(), [](const Task& lhs, const Task& rhs) {
    return lhs.arrival_time < rhs.arrival_time;
  });
  for (std::size_t i = 0; i < tasks.size(); ++i) {
    tasks[i].id = static_cast<TaskId>(i);
  }

  scheduler.reset(tasks);
  std::vector<RuntimeBookkeeping> state(tasks.size());
  std::size_t next_arrival = 0;
  std::size_t finished = 0;
  Time now = tasks.empty() ? 0 : tasks.front().arrival_time;
  Time busy_time = 0;
  std::uint64_t context_switches = 0;
  std::optional<TaskId> running;
  Time current_slice_ticks = 0;

  while (finished < tasks.size() && now < config.max_time) {
    while (next_arrival < tasks.size() && tasks[next_arrival].arrival_time <= now) {
      state[next_arrival].arrived = true;
      scheduler.on_task_ready(tasks[next_arrival].id, tasks[next_arrival], now);
      ++next_arrival;
    }
    for (std::size_t i = 0; i < tasks.size(); ++i) {
      if (state[i].blocked && state[i].unblock_time <= now) {
        state[i].blocked = false;
        scheduler.on_task_ready(tasks[i].id, tasks[i], now);
      }
    }

    if (!running) {
      running = scheduler.pick_next(now, tasks);
      current_slice_ticks = 0;
      if (running) {
        ++context_switches;
        now += config.context_switch_cost;
      }
    }

    if (!running) {
      const Time next = next_event_time(tasks, state, next_arrival);
      if (next == std::numeric_limits<Time>::max()) {
        break;
      }
      now = std::max(now + 1, next);
      continue;
    }

    const std::size_t idx = static_cast<std::size_t>(*running);
    auto& task = tasks[idx];
    auto& item = state[idx];
    if (!item.has_started) {
      item.has_started = true;
      item.first_start_time = now;
    }

    task.remaining_time -= 1;
    task.ran_since_io += 1;
    busy_time += 1;
    current_slice_ticks += 1;
    now += 1;

    if (task.remaining_time == 0) {
      item.finished = true;
      item.finish_time = now;
      scheduler.on_task_finished(task.id, task, now);
      running.reset();
      ++finished;
      continue;
    }

    if (task.io_interval > 0 && task.ran_since_io >= task.io_interval) {
      task.ran_since_io = 0;
      item.blocked = true;
      item.unblock_time = now + task.io_duration;
      scheduler.on_task_blocked(task.id, task, now);
      running.reset();
      continue;
    }

    if (scheduler.should_preempt(task.id, task, now, current_slice_ticks)) {
      item.preemptions += 1;
      scheduler.on_task_preempted(task.id, task, now);
      running.reset();
      current_slice_ticks = 0;
    }
  }

  if (finished < tasks.size()) {
    throw std::runtime_error("simulation exceeded max_time before all tasks finished");
  }

  std::vector<TaskResult> results;
  results.reserve(tasks.size());
  double total_turnaround = 0.0;
  double total_waiting = 0.0;
  double total_response = 0.0;
  std::size_t missed = 0;

  for (std::size_t i = 0; i < tasks.size(); ++i) {
    const auto& task = tasks[i];
    const auto& item = state[i];
    const Time turnaround = item.finish_time - task.arrival_time;
    const Time waiting = turnaround > task.total_runtime ? turnaround - task.total_runtime : 0;
    const Time response = item.has_started && item.first_start_time >= task.arrival_time ? item.first_start_time - task.arrival_time : 0;
    const bool missed_deadline = item.finish_time > task.deadline;
    total_turnaround += static_cast<double>(turnaround);
    total_waiting += static_cast<double>(waiting);
    total_response += static_cast<double>(response);
    missed += missed_deadline ? 1 : 0;
    results.push_back(TaskResult{
        .id = task.id,
        .arrival_time = task.arrival_time,
        .start_time = item.first_start_time,
        .finish_time = item.finish_time,
        .deadline = task.deadline,
        .total_runtime = task.total_runtime,
        .preemptions = item.preemptions,
        .missed_deadline = missed_deadline,
    });
  }

  const double count = static_cast<double>(tasks.size());
  RunSummary summary{
      .scheduler_name = std::string(scheduler.name()),
      .input_label = std::move(input_label),
      .task_count = tasks.size(),
      .throughput_tasks_per_time = now > 0 ? count / static_cast<double>(now) : 0.0,
      .avg_turnaround_time = count > 0 ? total_turnaround / count : 0.0,
      .avg_waiting_time = count > 0 ? total_waiting / count : 0.0,
      .avg_response_time = count > 0 ? total_response / count : 0.0,
      .deadline_miss_rate = count > 0 ? static_cast<double>(missed) / count : 0.0,
      .cpu_utilization = now > 0 ? static_cast<double>(busy_time) / static_cast<double>(now) : 0.0,
      .context_switches = context_switches,
      .makespan = now,
  };

  return {.summary = std::move(summary), .tasks = std::move(results)};
}

void write_summary_csv(const std::string& output_path, const std::vector<RunSummary>& summaries) {
  std::filesystem::create_directories(std::filesystem::path(output_path).parent_path());
  std::ofstream output(output_path);
  if (!output) {
    throw std::runtime_error("failed to write summary CSV: " + output_path);
  }
  output << "scheduler,input,tasks,throughput,avg_turnaround,avg_waiting,avg_response,deadline_miss_rate,cpu_utilization,context_switches,makespan\n";
  for (const auto& row : summaries) {
    output << row.scheduler_name << ','
           << row.input_label << ','
           << row.task_count << ','
           << row.throughput_tasks_per_time << ','
           << row.avg_turnaround_time << ','
           << row.avg_waiting_time << ','
           << row.avg_response_time << ','
           << row.deadline_miss_rate << ','
           << row.cpu_utilization << ','
           << row.context_switches << ','
           << row.makespan << '\n';
  }
}

void write_task_results_csv(const std::string& output_path, const SimulationOutput& output_data) {
  std::filesystem::create_directories(std::filesystem::path(output_path).parent_path());
  std::ofstream output(output_path);
  if (!output) {
    throw std::runtime_error("failed to write task CSV: " + output_path);
  }
  output << "task_id,arrival,start,finish,deadline,runtime,preemptions,missed_deadline\n";
  for (const auto& row : output_data.tasks) {
    output << row.id << ','
           << row.arrival_time << ','
           << row.start_time << ','
           << row.finish_time << ','
           << row.deadline << ','
           << row.total_runtime << ','
           << row.preemptions << ','
           << (row.missed_deadline ? 1 : 0) << '\n';
  }
}

}  // namespace sched
