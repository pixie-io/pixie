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

#include "src/common/ch/utils.h"

#include <exception>
#include <memory>
#include <string>
#include <utility>

namespace px {
namespace ch {

StatusOr<std::unique_ptr<::clickhouse::Client>> CreateClient(
    const ::clickhouse::ClientOptions& options) {
  try {
    return std::make_unique<::clickhouse::Client>(options);
  } catch (const std::exception& e) {
    return error::Internal("Failed to create ClickHouse client: $0", e.what());
  }
}

Status Execute(::clickhouse::Client* client, const std::string& query) {
  try {
    client->Execute(query);
  } catch (const std::exception& e) {
    return error::Internal("Failed to execute ClickHouse statement: $0", e.what());
  }
  return Status::OK();
}

Status Select(::clickhouse::Client* client, const std::string& query,
              std::function<void(const ::clickhouse::Block&)> block_cb) {
  try {
    client->Select(query, std::move(block_cb));
  } catch (const std::exception& e) {
    return error::Internal("Failed to execute ClickHouse query: $0", e.what());
  }
  return Status::OK();
}

Status Insert(::clickhouse::Client* client, const std::string& table_name,
              const ::clickhouse::Block& block) {
  try {
    client->Insert(table_name, block);
  } catch (const std::exception& e) {
    return error::Internal("Failed to insert into ClickHouse table $0: $1", table_name, e.what());
  }
  return Status::OK();
}

}  // namespace ch
}  // namespace px
