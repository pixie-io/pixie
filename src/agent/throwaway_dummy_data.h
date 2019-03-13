#pragma once
#include <memory>

#include "src/carnot/exec/table.h"
#include "src/common/status.h"

namespace pl {
namespace agent {

/**
 * @brief Table representing a few hours of dummy data for hipster shop.
 *
 * @return StatusOr<std::shared_ptr<Table>>
 */
StatusOr<std::shared_ptr<carnot::exec::Table>> FakeHipsterTable();

}  // namespace agent
}  // namespace pl
