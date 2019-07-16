#pragma once

#include <string>

#include "absl/numeric/int128.h"

namespace pl {
namespace md {

/**
 * UID refers to the unique IDs used by K8s. These IDs are
 * unique in both space and time.
 *
 * K8s stores UID as both regular strings and UUIDs.
 */
using UID = std::string;

/**
 * CID refers to a unique container ID. This ID is unique in both
 * space and time.
 */
using CID = std::string;

/**
 * Unique PIDs refers to uniquefied pids. They are unique in both
 * space and time. The format for a unique PID is:
 *
 * ------------------------------------------------------------
 * | 32-bit Agent ID |  32-bit PID  |     64-bit Start TS     |
 * ------------------------------------------------------------
 */
using UPID = absl::uint128;

}  // namespace md
}  // namespace pl
