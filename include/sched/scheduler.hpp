#pragma once

#include <optional>
#include <span>
#include <string_view>

#include "sched/types.hpp"

namespace sched {

class IScheduler {
 public:
  virtual ~IScheduler() = default;
  virtual std::string_view name() const = 0;
  virtual void reset(std::span<const Task> tasks) = 0;
  virtual void on_task_ready(TaskId id, const Task& task, Time now) = 0;
  virtual void on_task_preempted(TaskId id, const Task& task, Time now) = 0;
  virtual void on_task_blocked(TaskId id, const Task& task, Time now) = 0;
  virtual void on_task_finished(TaskId id, const Task& task, Time now) = 0;
  virtual std::optional<TaskId> pick_next(Time now, std::span<const Task> tasks) = 0;
  virtual bool should_preempt(TaskId running, const Task& task, Time now, Time current_slice_ticks) = 0;
};

class FifoScheduler final : public IScheduler {
 public:
  std::string_view name() const override { return "fifo"; }
  void reset(std::span<const Task> tasks) override;
  void on_task_ready(TaskId id, const Task& task, Time now) override;
  void on_task_preempted(TaskId id, const Task& task, Time now) override;
  void on_task_blocked(TaskId id, const Task& task, Time now) override;
  void on_task_finished(TaskId id, const Task& task, Time now) override;
  std::optional<TaskId> pick_next(Time now, std::span<const Task> tasks) override;
  bool should_preempt(TaskId running, const Task& task, Time now, Time current_slice_ticks) override;

 private:
  std::vector<TaskId> ready_{};
  std::size_t head_{0};
};

class MlfqScheduler final : public IScheduler {
 public:
  explicit MlfqScheduler(std::vector<Time> quantums = {2, 4, 8});
  std::string_view name() const override { return "mlfq"; }
  void reset(std::span<const Task> tasks) override;
  void on_task_ready(TaskId id, const Task& task, Time now) override;
  void on_task_preempted(TaskId id, const Task& task, Time now) override;
  void on_task_blocked(TaskId id, const Task& task, Time now) override;
  void on_task_finished(TaskId id, const Task& task, Time now) override;
  std::optional<TaskId> pick_next(Time now, std::span<const Task> tasks) override;
  bool should_preempt(TaskId running, const Task& task, Time now, Time current_slice_ticks) override;

 private:
  void push(TaskId id, std::size_t level);

  std::vector<Time> quantums_{};
  std::vector<std::vector<TaskId>> queues_{};
  std::vector<std::size_t> heads_{};
  std::vector<std::size_t> levels_by_task_{};
};

class RuntimePredictor;

class MlGuidedScheduler final : public IScheduler {
 public:
  explicit MlGuidedScheduler(const RuntimePredictor& predictor);
  std::string_view name() const override { return "ml_guided"; }
  void reset(std::span<const Task> tasks) override;
  void on_task_ready(TaskId id, const Task& task, Time now) override;
  void on_task_preempted(TaskId id, const Task& task, Time now) override;
  void on_task_blocked(TaskId id, const Task& task, Time now) override;
  void on_task_finished(TaskId id, const Task& task, Time now) override;
  std::optional<TaskId> pick_next(Time now, std::span<const Task> tasks) override;
  bool should_preempt(TaskId running, const Task& task, Time now, Time current_slice_ticks) override;

 private:
  const RuntimePredictor& predictor_;
  std::vector<TaskId> ready_{};
};

}  // namespace sched
