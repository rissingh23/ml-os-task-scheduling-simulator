#include "sched/types.hpp"

#include <stdexcept>

namespace sched {

std::string_view to_string(WorkloadProfile profile) {
  switch (profile) {
    case WorkloadProfile::Balanced:
      return "balanced";
    case WorkloadProfile::DeadlineTight:
      return "deadline_tight";
    case WorkloadProfile::IoHeavy:
      return "io_heavy";
    case WorkloadProfile::Bursty:
      return "bursty";
  }
  return "unknown";
}

std::string_view to_string(SchedulerKind kind) {
  switch (kind) {
    case SchedulerKind::Fifo:
      return "fifo";
    case SchedulerKind::Mlfq:
      return "mlfq";
    case SchedulerKind::MlGuided:
      return "ml_guided";
  }
  return "unknown";
}

WorkloadProfile parse_profile(std::string_view value) {
  if (value == "balanced") {
    return WorkloadProfile::Balanced;
  }
  if (value == "deadline_tight" || value == "deadline-tight") {
    return WorkloadProfile::DeadlineTight;
  }
  if (value == "io_heavy" || value == "io-heavy") {
    return WorkloadProfile::IoHeavy;
  }
  if (value == "bursty") {
    return WorkloadProfile::Bursty;
  }
  throw std::runtime_error("unknown workload profile");
}

SchedulerKind parse_scheduler_kind(std::string_view value) {
  if (value == "fifo") {
    return SchedulerKind::Fifo;
  }
  if (value == "mlfq") {
    return SchedulerKind::Mlfq;
  }
  if (value == "ml_guided" || value == "ml-guided" || value == "ml") {
    return SchedulerKind::MlGuided;
  }
  throw std::runtime_error("unknown scheduler kind");
}

}  // namespace sched
