#pragma once
#include <memory>

#include "src/carnot/schema/table.h"
#include "src/common/status.h"

namespace pl {
namespace agent {

/**
 * @brief Table representing a few hours of dummy data for hipster shop.
 *
 * @return StatusOr<std::shared_ptr<Table>>
 */
StatusOr<std::shared_ptr<carnot::schema::Table>> FakeHipsterTable();

}  // namespace agent
}  // namespace pl
