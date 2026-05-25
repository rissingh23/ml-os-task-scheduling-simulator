#include "sched/ml_model.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace sched {

RuntimePredictor::RuntimePredictor(std::array<double, kFeatureCount> weights, double bias)
    : weights_(weights), bias_(bias) {}

RuntimePredictor::RuntimePredictor(std::unordered_map<TaskId, double> batch_predictions, std::string model_name)
    : batch_predictions_(std::move(batch_predictions)), model_name_(std::move(model_name)) {}

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
  const auto prediction_it = batch_predictions_.find(task.id);
  if (prediction_it != batch_predictions_.end()) {
    return std::max(1.0, prediction_it->second);
  }

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

RuntimePredictor load_batch_runtime_predictions(const std::string& path) {
  if (path.empty()) {
    return RuntimePredictor{};
  }

  std::ifstream input(path);
  if (!input) {
    throw std::runtime_error("failed to open batch prediction CSV: " + path);
  }

  std::string line;
  std::unordered_map<TaskId, double> predictions;
  bool first_line = true;
  while (std::getline(input, line)) {
    if (line.empty() || line[0] == '#') {
      continue;
    }
    if (first_line) {
      first_line = false;
      if (line.find("task_id") != std::string::npos) {
        continue;
      }
    }

    std::stringstream ss(line);
    std::string id_field;
    std::string prediction_field;
    if (!std::getline(ss, id_field, ',') || !std::getline(ss, prediction_field, ',')) {
      continue;
    }
    predictions.emplace(static_cast<TaskId>(std::stoull(id_field)), std::stod(prediction_field));
  }

  if (predictions.empty()) {
    throw std::runtime_error("batch prediction CSV did not contain any predictions");
  }

  return RuntimePredictor(std::move(predictions), "xgboost_batch_predictions");
}

}  // namespace sched
