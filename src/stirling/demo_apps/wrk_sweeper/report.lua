done = function(summary, latency, requests)
  -- open output file
  f = io.open("result.csv", "a+")
  rps = 1000000 * summary["requests"] / summary["duration"]
  header = "latency_min, latency_max, latency_mean, latency_stddev, latency_p50, latency_p90"
  header = header .. ", latency_p99, duration, requests, bytes, requests_per_sec\n"
  f:write(header)
  f:write(string.format("%f,%f,%f,%f,%f,%f,%f,%d,%d,%d,%f\n",
  latency.min, latency.max, latency.mean, latency.stdev, latency:percentile(50),
  latency:percentile(90), latency:percentile(99),
  summary["duration"], summary["requests"], summary["bytes"], rps))

  f:close()
end
