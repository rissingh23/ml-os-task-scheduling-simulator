#include "sched/ml_model.hpp"
#include "sched/scheduler.hpp"
#include "sched/simulator.hpp"
#include "sched/workload.hpp"

#include <cassert>
#include <span>
#include <vector>

namespace {

std::vector<sched::Task> tiny_workload() {
  return {
      {.id = 0, .arrival_time = 0, .total_runtime = 5, .remaining_time = 5, .deadline = 12, .priority = 1},
      {.id = 1, .arrival_time = 1, .total_runtime = 2, .remaining_time = 2, .deadline = 5, .priority = 4},
      {.id = 2, .arrival_time = 2, .total_runtime = 3, .remaining_time = 3, .deadline = 9, .priority = 2},
  };
}

}  // namespace

int main() {
  {
    std::vector<sched::Task> tasks = tiny_workload();
    sched::FifoScheduler scheduler;
    scheduler.reset(tasks);
    scheduler.on_task_ready(0, tasks[0], 0);
    scheduler.on_task_ready(1, tasks[1], 1);
    scheduler.on_task_ready(2, tasks[2], 2);
    assert(scheduler.pick_next(3, tasks) == 0);
    assert(scheduler.pick_next(3, tasks) == 1);
    assert(scheduler.pick_next(3, tasks) == 2);
  }

  {
    std::vector<sched::Task> tasks = tiny_workload();
    sched::MlfqScheduler scheduler({2, 4, 8});
    scheduler.reset(tasks);
    scheduler.on_task_ready(0, tasks[0], 0);
    assert(scheduler.pick_next(0, tasks) == 0);
    assert(!scheduler.should_preempt(0, tasks[0], 1, 1));
    assert(scheduler.should_preempt(0, tasks[0], 2, 2));
    scheduler.on_task_preempted(0, tasks[0], 2);
    scheduler.on_task_ready(1, tasks[1], 2);
    assert(scheduler.pick_next(2, tasks) == 1);
    assert(scheduler.pick_next(2, tasks) == 0);
  }

  {
    std::vector<sched::Task> tasks = {
        {.id = 0, .arrival_time = 0, .total_runtime = 80, .remaining_time = 80, .deadline = 120, .priority = 0},
        {.id = 1, .arrival_time = 0, .total_runtime = 8, .remaining_time = 8, .deadline = 30, .priority = 4},
        {.id = 2, .arrival_time = 0, .total_runtime = 15, .remaining_time = 15, .deadline = 90, .priority = 1},
    };
    sched::RuntimePredictor predictor;
    sched::MlGuidedScheduler scheduler(predictor);
    scheduler.reset(tasks);
    scheduler.on_task_ready(0, tasks[0], 0);
    scheduler.on_task_ready(1, tasks[1], 0);
    scheduler.on_task_ready(2, tasks[2], 0);
    assert(scheduler.pick_next(0, tasks) == 1);
    assert(scheduler.should_preempt(1, tasks[1], 16, 16));
  }

  {
    sched::FifoScheduler scheduler;
    const auto output = sched::run_simulation(scheduler, "tiny", tiny_workload(), {.context_switch_cost = 0});
    assert(output.tasks.size() == 3);
    assert(output.summary.task_count == 3);
    assert(output.summary.makespan == 10);
  }

  {
    sched::MlfqScheduler scheduler;
    const auto output = sched::run_simulation(scheduler, "tiny", tiny_workload(), {.context_switch_cost = 0});
    assert(output.summary.context_switches > 0);
    assert(output.summary.avg_turnaround_time > 0.0);
  }

  {
    sched::RuntimePredictor predictor;
    sched::MlGuidedScheduler scheduler(predictor);
    const auto output = sched::run_simulation(scheduler, "tiny", tiny_workload(), {.context_switch_cost = 0});
    assert(output.summary.scheduler_name == "ml_guided");
    assert(output.summary.deadline_miss_rate >= 0.0);
  }

  {
    const auto tasks = sched::generate_workload(sched::WorkloadProfile::IoHeavy, 100, 7);
    sched::MlfqScheduler scheduler;
    const auto output = sched::run_simulation(scheduler, "io", tasks);
    assert(output.tasks.size() == 100);
    assert(output.summary.cpu_utilization > 0.0);
  }

  return 0;
}
