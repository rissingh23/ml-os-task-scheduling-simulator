#pragma once

#include <vector>

#include "sched/scheduler.hpp"
#include "sched/types.hpp"

namespace sched {

struct SimulationConfig {
  Time context_switch_cost{1};
  Time max_time{10'000'000};
};

struct SimulationOutput {
  RunSummary summary{};
  std::vector<TaskResult> tasks{};
};

SimulationOutput run_simulation(
    IScheduler& scheduler,
    std::string input_label,
    const std::vector<Task>& input_tasks,
    SimulationConfig config = {});

void write_summary_csv(const std::string& output_path, const std::vector<RunSummary>& summaries);
void write_task_results_csv(const std::string& output_path, const SimulationOutput& output);

}  // namespace sched
