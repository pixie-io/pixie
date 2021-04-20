/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <gflags/gflags.h>

#include <benchmark/benchmark.h>
#include "src/carnot/funcs/builtins/sql_parsing/normalization.h"
#include "src/common/perf/perf.h"

// NOLINTNEXTLINE : runtime/references.
static void BM_NormalizePgSQL(benchmark::State& state, std::string query) {
  for (auto _ : state) {
    benchmark::DoNotOptimize(px::carnot::builtins::sql_parsing::normalize_pgsql(query, {}));
  }
}

// NOLINTNEXTLINE : runtime/references.
static void BM_NormalizeMySQL(benchmark::State& state, std::string query) {
  for (auto _ : state) {
    benchmark::DoNotOptimize(px::carnot::builtins::sql_parsing::normalize_mysql(query, {}));
  }
}

BENCHMARK_CAPTURE(BM_NormalizePgSQL, select,
                  "SELECT * FROM test WHERE property=1234 AND property2='abcd'");
BENCHMARK_CAPTURE(BM_NormalizePgSQL, select_1, "SELECT 1");
BENCHMARK_CAPTURE(BM_NormalizePgSQL, begin, "BEGIN;");
BENCHMARK_CAPTURE(BM_NormalizePgSQL, create_table,
                  "CREATE TABLE test (name varchar(20), address text, foo int, bar text)");
BENCHMARK_CAPTURE(BM_NormalizePgSQL, update, "UPDATE test SET age=10 where name='abcd'");
BENCHMARK_CAPTURE(BM_NormalizePgSQL, compare, "SELECT * from test where abcd >= 11");
BENCHMARK_CAPTURE(BM_NormalizePgSQL, select_into,
                  "SELECT 'abcd' as col1, 1234 as col2, 1.2345 as col3 into my_new_table");
BENCHMARK_CAPTURE(BM_NormalizePgSQL, func, "SELECT length(abcd) + 1 from test");
BENCHMARK_CAPTURE(
    BM_NormalizePgSQL, insert_into,
    R"(INSERT INTO test (a, b, c, d, e) VALUES (1, 'abcd', 1.23, true, E'\\xDEADBEEF'))");

BENCHMARK_CAPTURE(BM_NormalizeMySQL, select,
                  "SELECT * FROM test WHERE property=1234 AND property2='abcd'");
BENCHMARK_CAPTURE(BM_NormalizeMySQL, select_1, "SELECT 1");
BENCHMARK_CAPTURE(BM_NormalizeMySQL, begin, "BEGIN;");
BENCHMARK_CAPTURE(BM_NormalizeMySQL, create_table,
                  "CREATE TABLE test (name text, address text, foo int, bar text)");
BENCHMARK_CAPTURE(BM_NormalizeMySQL, update, "UPDATE test SET age=10 where name='abcd'");
BENCHMARK_CAPTURE(BM_NormalizeMySQL, compare, "SELECT * from test where abcd >= 11");
BENCHMARK_CAPTURE(BM_NormalizeMySQL, select_into,
                  "INSERT INTO my_new_table SELECT 'abcd' as col1, 1234 as col2, 1.2345 as col3");
BENCHMARK_CAPTURE(BM_NormalizeMySQL, func, "SELECT length(abcd) + 1 from test");
BENCHMARK_CAPTURE(
    BM_NormalizeMySQL, insert_into,
    R"(INSERT INTO test (a, b, c, d, e) VALUES (1, 'abcd', 1.23, true, X'DEADBEEF'))");
BENCHMARK_CAPTURE(BM_NormalizeMySQL, sock_shop,
                  "SELECT sock.sock_id AS id, sock.name, sock.description, sock.price, sock.count, "
                  "sock.image_url_1, sock.image_url_2, GROUP_CONCAT(tag.name) AS tag_name FROM "
                  "sock "
                  "JOIN sock_tag ON sock.sock_id=sock_tag.sock_id JOIN tag ON "
                  "sock_tag.tag_id=tag.tag_id "
                  "WHERE sock.sock_id =abcde GROUP BY sock.sock_id;");
