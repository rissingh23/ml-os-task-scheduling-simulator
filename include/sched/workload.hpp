#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "sched/types.hpp"

namespace sched {

std::vector<Task> generate_workload(WorkloadProfile profile, std::size_t task_count, std::uint64_t seed);
std::vector<Task> load_tasks_from_csv(const std::string& path, std::size_t limit = 0);
void write_feature_dataset_csv(const std::string& output_path, const std::vector<Task>& tasks);

}  // namespace sched
