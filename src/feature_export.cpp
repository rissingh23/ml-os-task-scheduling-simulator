#include "sched/workload.hpp"

#include <filesystem>
#include <fstream>
#include <stdexcept>

#include "sched/ml_model.hpp"

namespace sched {

void write_feature_dataset_csv(const std::string& output_path, const std::vector<Task>& tasks) {
  std::filesystem::create_directories(std::filesystem::path(output_path).parent_path());
  std::ofstream output(output_path);
  if (!output) {
    throw std::runtime_error("failed to write feature dataset: " + output_path);
  }

  output << "task_id,arrival_time,requested_runtime,deadline_slack,priority,io_interval,io_duration,label_runtime\n";
  for (const auto& task : tasks) {
    const auto features = extract_features(task, task.arrival_time);
    output << features.task_id << ','
           << features.arrival_time << ','
           << features.requested_runtime << ','
           << features.deadline_slack << ','
           << features.priority << ','
           << features.io_interval << ','
           << features.io_duration << ','
           << task.total_runtime << '\n';
  }
}

}  // namespace sched
