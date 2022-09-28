# Stirling experimental code

## signal_handler.c

This is a trivial program that installs a signal handler of SIGINT.
We have used it to verify the behavior of proc_exit_tracer.
Build it with `clang++ -o signal_hanlder signal_handler.cc && ./signal_handler`
