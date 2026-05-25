#include "sched/scheduler.hpp"

#include <algorithm>

namespace sched {

void FifoScheduler::reset(std::span<const Task>) {
  ready_.clear();
  head_ = 0;
}

void FifoScheduler::on_task_ready(TaskId id, const Task&, Time) {
  ready_.push_back(id);
}

void FifoScheduler::on_task_preempted(TaskId id, const Task&, Time) {
  ready_.push_back(id);
}

void FifoScheduler::on_task_blocked(TaskId, const Task&, Time) {}

void FifoScheduler::on_task_finished(TaskId, const Task&, Time) {}

std::optional<TaskId> FifoScheduler::pick_next(Time, std::span<const Task>) {
  while (head_ < ready_.size()) {
    return ready_[head_++];
  }
  ready_.clear();
  head_ = 0;
  return std::nullopt;
}

bool FifoScheduler::should_preempt(TaskId, const Task&, Time, Time) {
  return false;
}

}  // namespace sched
