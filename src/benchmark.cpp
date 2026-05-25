#include "sched/benchmark.hpp"

#include <memory>

#include "sched/ml_model.hpp"
#include "sched/scheduler.hpp"
#include "sched/simulator.hpp"

namespace sched {

std::vector<RunSummary> run_all_schedulers(
    const std::vector<Task>& tasks,
    std::string input_label,
    const std::string& model_path) {
  const auto predictor = load_runtime_predictor(model_path);
  FifoScheduler fifo;
  MlfqScheduler mlfq;
  MlGuidedScheduler ml(predictor);

  std::vector<RunSummary> summaries;
  summaries.push_back(run_simulation(fifo, input_label, tasks).summary);
  summaries.push_back(run_simulation(mlfq, input_label, tasks).summary);
  summaries.push_back(run_simulation(ml, input_label, tasks).summary);
  return summaries;
}

}  // namespace sched
