#include "sched/ml_model.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace sched {

RuntimePredictor::RuntimePredictor(std::array<double, kFeatureCount> weights, double bias)
    : weights_(weights), bias_(bias) {}

TaskFeatures extract_features(const Task& task, Time now) {
  const double slack = task.deadline > now ? static_cast<double>(task.deadline - now) : 0.0;
  return {
      .task_id = task.id,
      .arrival_time = task.arrival_time,
      .requested_runtime = static_cast<double>(task.remaining_time),
      .deadline_slack = slack,
      .priority = static_cast<double>(task.priority),
      .io_interval = static_cast<double>(task.io_interval),
      .io_duration = static_cast<double>(task.io_duration),
  };
}

double RuntimePredictor::predict_runtime(const Task& task, Time now) const {
  const auto features = extract_features(task, now);
  const std::array<double, kFeatureCount> values{
      features.requested_runtime,
      features.deadline_slack,
      features.priority,
      features.io_interval,
      features.io_duration,
      static_cast<double>(task.ran_since_io),
  };

  double prediction = bias_;
  for (std::size_t i = 0; i < values.size(); ++i) {
    prediction += weights_[i] * values[i];
  }
  return std::max(1.0, prediction);
}

RuntimePredictor load_runtime_predictor(const std::string& path) {
  if (path.empty()) {
    return RuntimePredictor{};
  }

  std::ifstream input(path);
  if (!input) {
    throw std::runtime_error("failed to open model weights: " + path);
  }

  std::string line;
  std::array<double, kFeatureCount> weights{};
  double bias = 0.0;
  bool saw_weights = false;
  while (std::getline(input, line)) {
    if (line.empty() || line[0] == '#') {
      continue;
    }
    std::stringstream ss(line);
    std::string name;
    std::string value;
    if (!std::getline(ss, name, ',') || !std::getline(ss, value)) {
      continue;
    }
    if (name == "bias") {
      bias = std::stod(value);
      continue;
    }
    const std::size_t index = static_cast<std::size_t>(std::stoull(name));
    if (index < weights.size()) {
      weights[index] = std::stod(value);
      saw_weights = true;
    }
  }

  if (!saw_weights) {
    throw std::runtime_error("model file did not contain feature weights");
  }
  return RuntimePredictor(weights, bias);
}

}  // namespace sched
