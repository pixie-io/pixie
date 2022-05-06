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
#include <arpa/inet.h>
#include <netdb.h>

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <string>
#include <utility>
#include <vector>

#include "src/carnot/funcs/net/dns.h"
#include "src/carnot/udf/registry.h"
#include "src/carnot/udf/type_inference.h"
#include "src/common/base/inet_utils.h"
#include "src/shared/metadata/metadata_state.h"
#include "src/shared/types/types.h"

namespace px {
namespace carnot {
namespace funcs {
namespace net {

using ScalarUDF = px::carnot::udf::ScalarUDF;

class NSLookupUDF : public ScalarUDF {
 public:
  StringValue Exec(FunctionContext*, StringValue addr) { return cache_.Lookup(addr); }

  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Perform a DNS lookup for the value (experimental).")
        .Details("Experimental UDF to perform a DNS lookup for a given value.")
        .Arg("addr", "An IP address")
        .Example("df.hostname = px.nslookup(df.ip_addr)")
        .Returns("The hostname.");
  }

 private:
  internal::DNSCache& cache_ = internal::DNSCache::GetInstance();
};

class CIDRsContainIPUDF : public ScalarUDF {
 public:
  BoolValue Exec(FunctionContext*, StringValue cidrs_str, StringValue ip_addr) {
    // The expectation is that users will call this UDF with a constant cidrs_str, so cache to
    // prevent unnecessary parsing.
    if (cidrs_str != parsed_cidr_str_) {
      parsed_cidr_str_ = cidrs_str;

      rapidjson::Document doc;
      rapidjson::ParseResult ok = doc.Parse(cidrs_str.data());
      if (ok == nullptr) {
        return false;
      }
      if (!doc.IsArray()) {
        return false;
      }
      cidrs_.clear();
      for (rapidjson::Value::ConstValueIterator itr = doc.Begin(); itr != doc.End(); ++itr) {
        if (!itr->IsString()) {
          return false;
        }
        cidrs_.emplace_back();
        auto s = px::ParseCIDRBlock(itr->GetString(), &cidrs_.back());
        if (!s.ok()) {
          cidrs_.pop_back();
        }
      }
    }

    px::InetAddr addr = {};
    auto s = px::ParseIPAddress(ip_addr, &addr);
    if (!s.ok()) {
      return false;
    }
    for (const auto& cidr : cidrs_) {
      if (px::CIDRContainsIPAddr(cidr, addr)) {
        return true;
      }
    }
    return false;
  }
  static udf::ScalarUDFDocBuilder Doc() {
    return udf::ScalarUDFDocBuilder("Determine whether an IP is contained in a set of CIDR ranges.")
        .Details(
            "Determine whether the given IP is within anyone of the CIDR ranges provided. For "
            "example, 10.0.0.1 is contained in the CIDR range 10.0.0.0/24.")
        .Arg("cidrs",
             "Json array of CIDR ranges, where each CIDR range is a string of format "
             "'<IP>/<prefix_length>'")
        .Arg("ip_addr", "IP address to check for presence in range.")
        .Example(
            "df.cluster_cidrs = px.get_cidrs()"
            "| df.ip_is_in_cluster = px.cidrs_contain_ip(df.cluster_cidrs, df.remote_addr)")
        .Returns(
            "boolean representing whether the given IP is in any one of the given CIDR ranges.");
  }

 private:
  std::string parsed_cidr_str_ = "";
  std::vector<px::CIDRBlock> cidrs_;
};

void RegisterNetOpsOrDie(px::carnot::udf::Registry* registry);

}  // namespace net
}  // namespace funcs
}  // namespace carnot
}  // namespace px
