#include "sched/workload.hpp"

#include <algorithm>
#include <random>

namespace sched {

std::vector<Task> generate_workload(WorkloadProfile profile, std::size_t task_count, std::uint64_t seed) {
  std::mt19937_64 rng(seed);
  std::uniform_int_distribution<int> priority_dist(0, 4);
  std::uniform_int_distribution<int> runtime_balanced(4, 80);
  std::uniform_int_distribution<int> runtime_short(2, 24);
  std::uniform_int_distribution<int> runtime_long(40, 160);
  std::uniform_int_distribution<int> gap_balanced(8, 72);
  std::uniform_int_distribution<int> gap_tight(6, 48);
  std::uniform_int_distribution<int> gap_bursty(0, 1);

  std::vector<Task> tasks;
  tasks.reserve(task_count);
  Time now = 0;

  for (std::size_t i = 0; i < task_count; ++i) {
    Time runtime = 0;
    Time io_interval = 0;
    Time io_duration = 0;
    Time slack = 0;

    switch (profile) {
      case WorkloadProfile::Balanced:
        now += static_cast<Time>(gap_balanced(rng));
        runtime = static_cast<Time>(runtime_balanced(rng));
        slack = runtime + 40 + static_cast<Time>(runtime_balanced(rng));
        break;
      case WorkloadProfile::DeadlineTight:
        now += static_cast<Time>(gap_tight(rng));
        runtime = static_cast<Time>(runtime_balanced(rng));
        slack = runtime + 10 + static_cast<Time>(runtime_short(rng));
        break;
      case WorkloadProfile::IoHeavy:
        now += static_cast<Time>(gap_balanced(rng));
        runtime = static_cast<Time>(runtime_balanced(rng));
        io_interval = std::max<Time>(2, runtime / 4);
        io_duration = static_cast<Time>(2 + (rng() % 8));
        slack = runtime + io_duration * 3 + static_cast<Time>(runtime_short(rng));
        break;
      case WorkloadProfile::Bursty:
        if (i % 40 == 0) {
          now += 120;
        } else {
          now += static_cast<Time>(gap_bursty(rng) + 1);
        }
        runtime = (rng() % 10 == 0) ? static_cast<Time>(runtime_long(rng)) : static_cast<Time>(runtime_short(rng));
        slack = runtime + 30 + static_cast<Time>(runtime_balanced(rng));
        break;
    }

    tasks.push_back(Task{
        .id = static_cast<TaskId>(i),
        .arrival_time = now,
        .total_runtime = runtime,
        .remaining_time = runtime,
        .deadline = now + slack,
        .priority = static_cast<std::uint32_t>(priority_dist(rng)),
        .io_interval = io_interval,
        .io_duration = io_duration,
        .ran_since_io = 0,
    });
  }

  return tasks;
}

}  // namespace sched
