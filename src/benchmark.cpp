#include "sched/benchmark.hpp"

#include <fstream>
#include <memory>

#include "sched/ml_model.hpp"
#include "sched/scheduler.hpp"
#include "sched/simulator.hpp"

namespace sched {
namespace {

RuntimePredictor load_predictor_auto(const std::string& path) {
  if (path.empty()) {
    return load_runtime_predictor(path);
  }

  std::ifstream input(path);
  std::string first_line;
  while (std::getline(input, first_line)) {
    if (!first_line.empty() && first_line[0] != '#') {
      break;
    }
  }
  if (first_line.find("task_id") != std::string::npos || first_line.find("predicted_runtime") != std::string::npos) {
    return load_batch_runtime_predictions(path);
  }
  return load_runtime_predictor(path);
}

}  // namespace

std::vector<RunSummary> run_all_schedulers(
    const std::vector<Task>& tasks,
    std::string input_label,
    const std::string& model_path) {
  const auto predictor = load_predictor_auto(model_path);
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
