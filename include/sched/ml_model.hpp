#pragma once

#include <array>
#include <string>

#include "sched/types.hpp"

namespace sched {

inline constexpr std::size_t kFeatureCount = 6;

class RuntimePredictor {
 public:
  RuntimePredictor() = default;
  explicit RuntimePredictor(std::array<double, kFeatureCount> weights, double bias = 0.0);

  [[nodiscard]] double predict_runtime(const Task& task, Time now) const;
  [[nodiscard]] const std::array<double, kFeatureCount>& weights() const { return weights_; }

 private:
  std::array<double, kFeatureCount> weights_{{1.0, 0.0, -0.15, -0.25, 0.05, 0.0}};
  double bias_{0.0};
};

RuntimePredictor load_runtime_predictor(const std::string& path);
TaskFeatures extract_features(const Task& task, Time now);

}  // namespace sched
