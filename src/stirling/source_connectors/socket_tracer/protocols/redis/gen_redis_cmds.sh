#!/bin/bash -e

# Download the webpage with all command and argument names.
curl https://redis.io/commands > redis_commands

# List command names.
xmllint --html --xpath '//span[@class="command"]/text()' redis_commands | grep -o "\S.*\S" > redis_cmds

# List command argument names.
xmllint --html --xpath '//span[@class="command"]/text() | //span[@class="args"]/text()' \
  redis_commands | grep -o "\S.*\S" > redis_cmdargs

# bazel runs under a different PWD, so use $(pwd) to get the absolute path.
bazel run src/stirling/source_connectors/socket_tracer/protocols/redis:redis_cmds_format_generator \
  -- --redis_cmds="$(pwd)/redis_cmds" --redis_cmdargs="$(pwd)/redis_cmdargs"
