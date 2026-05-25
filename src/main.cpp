#include "sched/benchmark.hpp"
#include "sched/ml_model.hpp"
#include "sched/scheduler.hpp"
#include "sched/simulator.hpp"
#include "sched/workload.hpp"

#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

namespace {

std::string arg_value(int argc, char** argv, const std::string& flag, const std::string& fallback) {
  for (int i = 1; i < argc - 1; ++i) {
    if (argv[i] == flag) {
      return argv[i + 1];
    }
  }
  return fallback;
}

void print_usage() {
  std::cout << "modes: benchmark | simulate | export-features\n";
  std::cout << "synthetic: --profile balanced|deadline_tight|io_heavy|bursty --tasks N --seed S\n";
  std::cout << "trace: --trace path/to/tasks.csv [--trace-limit N]\n";
  std::cout << "schedulers: --scheduler fifo|mlfq|ml_guided --model results/runtime_linear_model.csv --batch-predictions results/xgb_predictions.csv\n";
}

void print_summary(const sched::RunSummary& summary) {
  std::cout << summary.scheduler_name
            << " | input=" << summary.input_label
            << " | tasks=" << summary.task_count
            << " | miss_rate=" << summary.deadline_miss_rate
            << " | avg_turnaround=" << summary.avg_turnaround_time
            << " | avg_waiting=" << summary.avg_waiting_time
            << " | avg_response=" << summary.avg_response_time
            << " | cpu_util=" << summary.cpu_utilization
            << " | context_switches=" << summary.context_switches
            << " | makespan=" << summary.makespan << '\n';
}

}  // namespace

int main(int argc, char** argv) {
  try {
    const std::string mode = arg_value(argc, argv, "--mode", "benchmark");
    const std::string trace_path = arg_value(argc, argv, "--trace", "");
    const std::size_t trace_limit = static_cast<std::size_t>(std::stoull(arg_value(argc, argv, "--trace-limit", "0")));
    const auto profile = sched::parse_profile(arg_value(argc, argv, "--profile", "balanced"));
    const auto scheduler_kind = sched::parse_scheduler_kind(arg_value(argc, argv, "--scheduler", "mlfq"));
    const std::size_t task_count = static_cast<std::size_t>(std::stoull(arg_value(argc, argv, "--tasks", "2000")));
    const std::uint64_t seed = std::stoull(arg_value(argc, argv, "--seed", "42"));
    const std::string output = arg_value(argc, argv, "--output", "results/scheduler_results.csv");
    const std::string model_path = arg_value(argc, argv, "--model", "");
    const std::string batch_predictions_path = arg_value(argc, argv, "--batch-predictions", "");

    const bool using_trace = !trace_path.empty();
    const std::string input_label = using_trace ? std::filesystem::path(trace_path).stem().string() : std::string(sched::to_string(profile));
    const auto tasks = using_trace ? sched::load_tasks_from_csv(trace_path, trace_limit) : sched::generate_workload(profile, task_count, seed);

    if (mode == "export-features") {
      sched::write_feature_dataset_csv(output, tasks);
      std::cout << "wrote features to " << output << '\n';
      return 0;
    }

    if (mode == "benchmark") {
      const auto summaries = sched::run_all_schedulers(tasks, input_label, batch_predictions_path.empty() ? model_path : batch_predictions_path);
      sched::write_summary_csv(output, summaries);
      for (const auto& summary : summaries) {
        print_summary(summary);
      }
      std::cout << "wrote benchmark CSV to " << output << '\n';
      return 0;
    }

    if (mode == "simulate") {
      const auto predictor = batch_predictions_path.empty()
          ? sched::load_runtime_predictor(model_path)
          : sched::load_batch_runtime_predictions(batch_predictions_path);
      sched::FifoScheduler fifo;
      sched::MlfqScheduler mlfq;
      sched::MlGuidedScheduler ml(predictor);
      sched::IScheduler* scheduler = &mlfq;
      if (scheduler_kind == sched::SchedulerKind::Fifo) {
        scheduler = &fifo;
      } else if (scheduler_kind == sched::SchedulerKind::MlGuided) {
        scheduler = &ml;
      }
      const auto result = sched::run_simulation(*scheduler, input_label, tasks);
      sched::write_task_results_csv(output, result);
      print_summary(result.summary);
      std::cout << "wrote task results to " << output << '\n';
      return 0;
    }

    print_usage();
    return 1;
  } catch (const std::exception& error) {
    std::cerr << "error: " << error.what() << '\n';
    print_usage();
    return 1;
  }
}
