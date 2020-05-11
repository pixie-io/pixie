#pragma once

#include <memory>
#include <string>
#include <utility>

#include "src/common/base/base.h"
#include "src/shared/types/types.h"
#include "src/stirling/data_table.h"

namespace pl {
namespace stirling {

// Example usage:
// RecordBuilder<&kTable> r(data_table);
// r.Append<r.ColIndex("field0")>(val0);
// r.Append<r.ColIndex("field1")>(val1);
// r.Append<r.ColIndex("field2")>(val2);
//
// NOTE: Today, the tabletization key must appear as replicated in a column.
// This is technically redundant information, as the column will simply contain a constant value.
// This is being done to keep Carnot simple. This way, Carnot does not to have to convert tablet
// keys into dynamically generated columns. For example, if the tablet key was a PID,
// the memory source from table store would have to add the PID when selecting multiple tablets,
// if the tablet key was not an explicit column.
// TODO(oazizi): Look into the optimization of avoiding replication.
// See https://phab.pixielabs.ai/D1428 for an abandoned implementation.

template <const DataTableSchema* schema>
class RecordBuilder {
 public:
  // Any string larger than this size will be truncated before being placed in the record.
  static constexpr int kMaxStringBytes = 8192;

  explicit RecordBuilder(DataTable* data_table, types::TabletIDView tablet_id)
      : RecordBuilder(data_table->ActiveRecordBatch(tablet_id)) {
    static_assert(schema->tabletized());
    tablet_id_ = tablet_id;
  }

  explicit RecordBuilder(DataTable* data_table) : RecordBuilder(data_table->ActiveRecordBatch()) {
    static_assert(!schema->tabletized());
  }

  // For convenience, a wrapper around ColIndex() in the DataTableSchema class.
  constexpr uint32_t ColIndex(std::string_view name) { return schema->ColIndex(name); }

  // The argument type is inferred by the table schema and the column index.
  template <const uint32_t index>
  inline void Append(
      typename types::DataTypeTraits<schema->elements()[index].type()>::value_type val) {
    if constexpr (index == schema->tabletization_key()) {
      // TODO(oazizi): This will probably break if val is ever StringValue.
      DCHECK(std::to_string(val.val) == tablet_id_);
    }

    if constexpr (std::is_same_v<
                      typename types::DataTypeTraits<schema->elements()[index].type()>::value_type,
                      types::StringValue>) {
      if (val.size() > kMaxStringBytes) {
        val.resize(kMaxStringBytes);
      }
    }

    record_batch_[index]->Append(std::move(val));
    DCHECK(!signature_[index]) << absl::Substitute(
        "Attempt to Append() to column $0 (name=$1) multiple times", index,
        schema->elements()[index].name().data());
    signature_.set(index);
  }

  ~RecordBuilder() {
    DCHECK(signature_.all()) << absl::Substitute(
        "Must call Append() on all columns. Column bitset = $0", signature_.to_string());
  }

 private:
  explicit RecordBuilder(types::ColumnWrapperRecordBatch* active_record_batch)
      : record_batch_(*active_record_batch) {}

  types::ColumnWrapperRecordBatch& record_batch_;
  std::bitset<schema->elements().size()> signature_;
  types::TabletIDView tablet_id_ = "";
};

}  // namespace stirling
}  // namespace pl
