#pragma once
#include <memory>

#include "src/common/base/base.h"
#include "src/table_store/table_store.h"

namespace pl {
namespace agent {

/**
 * @brief Table representing a few hours of dummy data for hipster shop.
 *
 * @return StatusOr<std::shared_ptr<Table>>
 */
StatusOr<std::shared_ptr<table_store::schema::Table>> FakeHipsterTable();

}  // namespace agent
}  // namespace pl
