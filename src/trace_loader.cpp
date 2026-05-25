#include "sched/workload.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

namespace sched {
namespace {

std::vector<std::string> split_csv_line(const std::string& line) {
  std::vector<std::string> fields;
  std::stringstream ss(line);
  std::string field;
  while (std::getline(ss, field, ',')) {
    fields.push_back(field);
  }
  return fields;
}

std::size_t index_or(const std::unordered_map<std::string, std::size_t>& header, const std::string& name, std::size_t fallback) {
  const auto it = header.find(name);
  return it == header.end() ? fallback : it->second;
}

}  // namespace

std::vector<Task> load_tasks_from_csv(const std::string& path, std::size_t limit) {
  std::ifstream input(path);
  if (!input) {
    throw std::runtime_error("failed to open task trace: " + path);
  }

  std::string line;
  if (!std::getline(input, line)) {
    return {};
  }

  std::unordered_map<std::string, std::size_t> header;
  const auto header_fields = split_csv_line(line);
  for (std::size_t i = 0; i < header_fields.size(); ++i) {
    header[header_fields[i]] = i;
  }

  const auto arrival_idx = index_or(header, "arrival_time", 0);
  const auto runtime_idx = index_or(header, "runtime", 1);
  const auto deadline_idx = index_or(header, "deadline", 2);
  const auto priority_idx = index_or(header, "priority", 3);
  const auto io_interval_idx = index_or(header, "io_interval", 4);
  const auto io_duration_idx = index_or(header, "io_duration", 5);

  std::vector<Task> tasks;
  while (std::getline(input, line)) {
    if (line.empty()) {
      continue;
    }
    const auto fields = split_csv_line(line);
    const auto get = [&](std::size_t idx, std::uint64_t fallback) {
      return idx < fields.size() && !fields[idx].empty() ? static_cast<std::uint64_t>(std::stoull(fields[idx])) : fallback;
    };
    const Time arrival = get(arrival_idx, 0);
    const Time runtime = std::max<std::uint64_t>(1, get(runtime_idx, 1));
    const Time deadline = get(deadline_idx, arrival + runtime * 2);
    tasks.push_back(Task{
        .id = static_cast<TaskId>(tasks.size()),
        .arrival_time = arrival,
        .total_runtime = runtime,
        .remaining_time = runtime,
        .deadline = deadline,
        .priority = static_cast<std::uint32_t>(get(priority_idx, 0)),
        .io_interval = get(io_interval_idx, 0),
        .io_duration = get(io_duration_idx, 0),
        .ran_since_io = 0,
    });
    if (limit > 0 && tasks.size() >= limit) {
      break;
    }
  }

  return tasks;
}

}  // namespace sched
