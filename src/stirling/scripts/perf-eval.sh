#!/bin/bash -e

# Here we invoke the perf-eval go program, along with other necessary sources,
# and just pass along all the cmd line args.
go run perf_eval_main.go "$@"
