#pragma once

#include <string>
#include <vector>

#include "sched/types.hpp"

namespace sched {

std::vector<RunSummary> run_all_schedulers(
    const std::vector<Task>& tasks,
    std::string input_label,
    const std::string& model_path = "");

}  // namespace sched
