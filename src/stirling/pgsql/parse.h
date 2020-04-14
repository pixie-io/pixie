#pragma once

#include <string_view>
#include <vector>

#include "src/stirling/common/parse_state.h"
#include "src/stirling/pgsql/types.h"

namespace pl {
namespace stirling {
namespace pgsql {

/**
 * Parse input data into messages.
 */
ParseState ParseRegularMessage(std::string_view* buf, RegularMessage* msg);

}  // namespace pgsql
}  // namespace stirling
}  // namespace pl
