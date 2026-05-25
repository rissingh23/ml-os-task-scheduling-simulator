#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace sched {

using TaskId = std::uint64_t;
using Time = std::uint64_t;

enum class WorkloadProfile {
  Balanced,
  DeadlineTight,
  IoHeavy,
  Bursty,
};

enum class SchedulerKind {
  Fifo,
  Mlfq,
  MlGuided,
};

struct Task {
  TaskId id{};
  Time arrival_time{};
  Time total_runtime{};
  Time remaining_time{};
  Time deadline{};
  std::uint32_t priority{};
  Time io_interval{};
  Time io_duration{};
  Time ran_since_io{};
};

struct TaskTraceRow {
  Task task{};
  double observed_runtime{};
  std::string source{};
};

struct TaskFeatures {
  TaskId task_id{};
  Time arrival_time{};
  double requested_runtime{};
  double deadline_slack{};
  double priority{};
  double io_interval{};
  double io_duration{};
};

struct TaskResult {
  TaskId id{};
  Time arrival_time{};
  Time start_time{};
  Time finish_time{};
  Time deadline{};
  Time total_runtime{};
  std::uint64_t preemptions{};
  bool missed_deadline{};
};

struct RunSummary {
  std::string scheduler_name{};
  std::string input_label{};
  std::size_t task_count{};
  double throughput_tasks_per_time{};
  double avg_turnaround_time{};
  double avg_waiting_time{};
  double avg_response_time{};
  double deadline_miss_rate{};
  double cpu_utilization{};
  std::uint64_t context_switches{};
  Time makespan{};
};

std::string_view to_string(WorkloadProfile profile);
std::string_view to_string(SchedulerKind kind);
WorkloadProfile parse_profile(std::string_view value);
SchedulerKind parse_scheduler_kind(std::string_view value);

}  // namespace sched
