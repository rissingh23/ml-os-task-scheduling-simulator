const TRAINED_RUNTIME_MODEL = {
  name: "trained_linear_runtime_v1",
  trainedOn: "12,000 synthetic balanced workload tasks exported by scheduler_sim",
  kind: "linear_regression",
  target: "observed task runtime",
  bias: -0.010932601888171689,
  features: [
    { name: "requested_runtime", label: "Burst", weight: 0.9996530342881648 },
    { name: "deadline_slack", label: "Deadline slack", weight: 0.0002057531870738085 },
    { name: "priority", label: "Priority", weight: -0.00002138567987713557 },
    { name: "io_interval", label: "I/O interval", weight: 0.0 },
    { name: "io_duration", label: "I/O duration", weight: 0.0 },
    { name: "ran_since_io", label: "Ran since I/O", weight: 0.0 },
  ],
};
