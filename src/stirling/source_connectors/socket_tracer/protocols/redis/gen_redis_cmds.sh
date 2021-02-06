#!/bin/bash -e

html_file="redis_commands.html"
redis_cmds_file="redis_cmds.txt"
redis_cmdargs_file="redis_cmdargs.txt"

# Download the webpage with all command and argument names.
curl https://redis.io/commands > ${html_file}

# List command names.
xmllint --html --xpath '//span[@class="command"]/text()' ${html_file} | grep -o "\S.*\S" > \
  ${redis_cmds_file}

# List command argument names.
xmllint --html --xpath '//span[@class="command"]/text() | //span[@class="args"]/text()' \
  ${html_file} | grep -o "\S.*\S" > ${redis_cmdargs_file}

# bazel runs under a different PWD, so use $(pwd) to get the absolute path.
bazel run src/stirling/source_connectors/socket_tracer/protocols/redis:redis_cmds_format_generator \
  -- --redis_cmds="$(pwd)/${redis_cmds_file}" --redis_cmdargs="$(pwd)/${redis_cmdargs_file}"

rm -f ${html_file} ${redis_cmds_file} ${redis_cmdargs_file}
