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

#pragma once

#include <clickhouse/client.h>

#include <functional>
#include <memory>
#include <string>

#include "src/common/base/base.h"

namespace px {
namespace ch {

// Status-returning wrappers around the exception-throwing clickhouse-cpp client API.

// Creates a ClickHouse client from the given options.
StatusOr<std::unique_ptr<::clickhouse::Client>> CreateClient(
    const ::clickhouse::ClientOptions& options);

// Executes a statement that returns no rows (e.g. DDL or a connection check).
Status Execute(::clickhouse::Client* client, const std::string& query);

// Executes a SELECT query, invoking block_cb for each returned block.
Status Select(::clickhouse::Client* client, const std::string& query,
              std::function<void(const ::clickhouse::Block&)> block_cb);

// Inserts a block into table_name.
Status Insert(::clickhouse::Client* client, const std::string& table_name,
              const ::clickhouse::Block& block);

}  // namespace ch
}  // namespace px
