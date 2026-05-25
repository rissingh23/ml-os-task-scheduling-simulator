#include "sched/scheduler.hpp"

#include <algorithm>
#include <limits>

#include "sched/ml_model.hpp"

namespace sched {

MlGuidedScheduler::MlGuidedScheduler(const RuntimePredictor& predictor) : predictor_(predictor) {}

void MlGuidedScheduler::reset(std::span<const Task>) {
  ready_.clear();
}

void MlGuidedScheduler::on_task_ready(TaskId id, const Task&, Time) {
  ready_.push_back(id);
}

void MlGuidedScheduler::on_task_preempted(TaskId id, const Task&, Time) {
  ready_.push_back(id);
}

void MlGuidedScheduler::on_task_blocked(TaskId, const Task&, Time) {}

void MlGuidedScheduler::on_task_finished(TaskId, const Task&, Time) {}

std::optional<TaskId> MlGuidedScheduler::pick_next(Time now, std::span<const Task> tasks) {
  if (ready_.empty()) {
    return std::nullopt;
  }

  auto best = ready_.begin();
  double best_score = std::numeric_limits<double>::infinity();
  for (auto it = ready_.begin(); it != ready_.end(); ++it) {
    const auto& task = tasks[static_cast<std::size_t>(*it)];
    const double predicted = predictor_.predict_runtime(task, now);
    const double slack = static_cast<double>(task.deadline > now ? task.deadline - now : 0);
    const double priority_bonus = static_cast<double>(task.priority) * 2.0;
    const double score = predicted + (0.65 * slack) - priority_bonus;
    if (score < best_score) {
      best_score = score;
      best = it;
    }
  }

  const TaskId chosen = *best;
  ready_.erase(best);
  return chosen;
}

bool MlGuidedScheduler::should_preempt(TaskId, const Task&, Time, Time current_slice_ticks) {
  return current_slice_ticks >= 16;
}

}  // namespace sched
