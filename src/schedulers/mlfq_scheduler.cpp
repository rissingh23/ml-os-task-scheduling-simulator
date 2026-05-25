#include "sched/scheduler.hpp"

#include <algorithm>

namespace sched {

MlfqScheduler::MlfqScheduler(std::vector<Time> quantums) : quantums_(std::move(quantums)) {
  if (quantums_.empty()) {
    quantums_ = {2, 4, 8};
  }
  queues_.resize(quantums_.size());
  heads_.resize(quantums_.size());
}

void MlfqScheduler::reset(std::span<const Task> tasks) {
  queues_.assign(quantums_.size(), {});
  heads_.assign(quantums_.size(), 0);
  levels_by_task_.assign(tasks.size(), 0);
}

void MlfqScheduler::push(TaskId id, std::size_t level) {
  const auto capped_level = std::min(level, quantums_.size() - 1);
  if (id >= levels_by_task_.size()) {
    levels_by_task_.resize(static_cast<std::size_t>(id) + 1, 0);
  }
  levels_by_task_[static_cast<std::size_t>(id)] = capped_level;
  queues_[capped_level].push_back(id);
}

void MlfqScheduler::on_task_ready(TaskId id, const Task&, Time) {
  push(id, id < levels_by_task_.size() ? levels_by_task_[static_cast<std::size_t>(id)] : 0);
}

void MlfqScheduler::on_task_preempted(TaskId id, const Task&, Time) {
  const auto current = id < levels_by_task_.size() ? levels_by_task_[static_cast<std::size_t>(id)] : 0;
  push(id, current + 1);
}

void MlfqScheduler::on_task_blocked(TaskId id, const Task&, Time) {
  if (id < levels_by_task_.size()) {
    levels_by_task_[static_cast<std::size_t>(id)] = 0;
  }
}

void MlfqScheduler::on_task_finished(TaskId, const Task&, Time) {}

std::optional<TaskId> MlfqScheduler::pick_next(Time, std::span<const Task>) {
  for (std::size_t level = 0; level < queues_.size(); ++level) {
    if (heads_[level] < queues_[level].size()) {
      return queues_[level][heads_[level]++];
    }
    queues_[level].clear();
    heads_[level] = 0;
  }
  return std::nullopt;
}

bool MlfqScheduler::should_preempt(TaskId id, const Task&, Time, Time current_slice_ticks) {
  const auto level = id < levels_by_task_.size() ? levels_by_task_[static_cast<std::size_t>(id)] : 0;
  return current_slice_ticks >= quantums_[std::min(level, quantums_.size() - 1)];
}

}  // namespace sched
