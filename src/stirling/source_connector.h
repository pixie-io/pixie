#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/common/base/base.h"
#include "src/common/system_config/system_config.h"
#include "src/shared/types/types.h"
#include "src/stirling/info_class_manager.h"

/**
 * These are the steps to follow to add a new data source connector.
 * 1. If required, create a new SourceConnector class.
 * 2. Add a new Create function with the following signature:
 *    static std::unique_ptr<SourceConnector> Create().
 *    In this function create an InfoClassSchema (vector of InfoClassElement)
 * 3. Register the data source in the appropriate registry.
 */

namespace pl {
namespace stirling {

class InfoClassManager;
class StirlingImpl;

#define DUMMY_SOURCE_CONNECTOR(NAME)                                        \
  class NAME : public SourceConnector {                                     \
   public:                                                                  \
    static constexpr bool kAvailable = false;                               \
    static constexpr auto kTables = ConstVectorView<DataTableSchema>();     \
    static std::unique_ptr<SourceConnector> Create(std::string_view name) { \
      PL_UNUSED(name);                                                      \
      return nullptr;                                                       \
    }                                                                       \
  }

class SourceConnector : public NotCopyable {
 public:
  /**
   * @brief Defines whether the SourceConnector has an implementation.
   *
   * Default in the base class is true, and normally should not be changed in the derived class.
   *
   * However, a dervived class may want to redefine to false in certain special circumstances:
   * 1) a SourceConnector that is just a placeholder (not yet implemented).
   * 2) a SourceConnector that is not compilable (e.g. on Mac). See DUMMY_SOURCE_CONNECTOR macro.
   */
  static constexpr bool kAvailable = true;

  static constexpr std::chrono::milliseconds kDefaultSamplingPeriod{100};
  static constexpr std::chrono::milliseconds kDefaultPushPeriod{1000};

  SourceConnector() = delete;
  virtual ~SourceConnector() = default;

  /**
   * @brief Initializes the source connector. Can only be called once.
   * @return Status of whether initialization was successful.
   */
  Status Init();

  /**
   * @brief Transfers any collected data, for the specified table, into the provided record_batch.
   * @param table_num The table number (id) of the data. See DataTableSchemas in individual
   * connectors.
   * @param record_batch The target to move the data into.
   */
  void TransferData(uint32_t table_num, types::ColumnWrapperRecordBatch* record_batch);

  /**
   * @brief Stops the source connector and releases any acquired resources.
   * May only be called after a successful Init().
   *
   * @return Status of whether stop was successful.
   */
  Status Stop();

  const std::string& source_name() const { return source_name_; }

  uint32_t num_tables() const { return table_schemas_.size(); }
  const ConstVectorView<DataElement> elements(uint32_t table_num) const {
    DCHECK_LT(table_num, num_tables())
        << absl::Substitute("Access to table out of bounds: table_num=$0", table_num);
    return table_schemas_[table_num].elements();
  }
  const std::string_view table_name(uint32_t table_num) const {
    DCHECK_LT(table_num, num_tables())
        << absl::Substitute("Access to table out of bounds: table_num=$0", table_num);
    return table_schemas_[table_num].name();
  }

  static constexpr uint32_t TableNum(ConstVectorView<DataTableSchema> tables,
                                     const DataTableSchema& key) {
    uint32_t i = 0;
    for (i = 0; i < tables.size(); i++) {
      if (tables[i].name() == key.name()) {
        break;
      }
    }

    // Check that we found the index. This prevents compilation if name is not found,
    // during constexpr evaluation (which is awesome!).
    COMPILE_TIME_ASSERT(i != tables.size(), "Could not find name");

    return i;
  }

  const std::chrono::milliseconds& default_sampling_period() { return default_sampling_period_; }
  const std::chrono::milliseconds& default_push_period() { return default_push_period_; }

  /**
   * @brief Utility function to convert time as recorded by in monotonic clock to real time.
   * This is especially useful for converting times from BPF, which are all in monotonic clock.
   */
  uint64_t ClockRealTimeOffset() {
    DCHECK(sysconfig_ != nullptr);
    return sysconfig_->ClockRealTimeOffset();
  }

  /**
   * Set pointer to stirling engine. This lifetime of the referenced value
   * needs to exceed the lifetime of the source connector.
   * @param stirling A pointer to the storling engine.
   */
  void set_stirling(const StirlingImpl* stirling) { stirling_ = stirling; }

  /**
   * Get the pointer to stirling engine. This pointers lifetime will be the greater than or equal
   * to the lifetime of the source connector.
   * @return a const pointer to stirling
   */
  const StirlingImpl* stirling() {
    CHECK(stirling_ != nullptr)
        << "Make sure set_stirling is called before running source connectors";
    return stirling_;
  }

 protected:
  explicit SourceConnector(std::string_view source_name,
                           const ConstVectorView<DataTableSchema>& table_schemas,
                           std::chrono::milliseconds default_sampling_period,
                           std::chrono::milliseconds default_push_period)
      : source_name_(source_name),
        table_schemas_(table_schemas),
        default_sampling_period_(default_sampling_period),
        default_push_period_(default_push_period) {}

  virtual Status InitImpl() = 0;
  virtual void TransferDataImpl(uint32_t table_num,
                                types::ColumnWrapperRecordBatch* record_batch) = 0;
  virtual Status StopImpl() = 0;

 protected:
  // Example usage:
  // RecordBuilder<&kTable> r(record_batch);
  // r.Append<r.ColIndex("field0")>(val0);
  // r.Append<r.ColIndex("field1")>(val1);
  // r.Append<r.ColIndex("field2")>(val2);
  template <const DataTableSchema* schema>
  class RecordBuilder {
   public:
    explicit RecordBuilder(types::ColumnWrapperRecordBatch* record_batch)
        : record_batch_(*record_batch) {}

    // For convenience, a wrapper around ColIndex() in the DataTableSchema class.
    constexpr uint32_t ColIndex(std::string_view name) { return schema->ColIndex(name); }

    // The argument type is inferred by the table schema and the column index.
    template <const uint32_t index>
    inline void Append(
        typename types::DataTypeTraits<schema->elements()[index].type()>::value_type val) {
      record_batch_[index]->Append(std::move(val));
      CHECK(!signature_[index]) << absl::Substitute(
          "Attempt to Append() to column $0 (name=$1) multiple times", index,
          schema->elements()[index].name().data());
      signature_.set(index);
    }

    ~RecordBuilder() {
      CHECK(signature_.all()) << absl::Substitute(
          "Must call Append() on all columns. Column bitset = $0", signature_.to_string());
    }

   private:
    types::ColumnWrapperRecordBatch& record_batch_;
    std::bitset<schema->elements().size()> signature_;
  };

 private:
  /**
   * Track state of connector. A connector's lifetime typically progresses sequentially
   * from kUninitialized -> kActive -> KStopped.
   *
   * kErrors is a special state to track a bad state.
   */
  enum class State { kUninitialized, kActive, kStopped, kErrors };

  State state_ = State::kUninitialized;

  const StirlingImpl* stirling_ = nullptr;
  const std::string source_name_;
  const ConstVectorView<DataTableSchema> table_schemas_;
  const std::chrono::milliseconds default_sampling_period_;
  const std::chrono::milliseconds default_push_period_;

  const common::SystemConfig* sysconfig_ = common::SystemConfig::GetInstance();
};

}  // namespace stirling
}  // namespace pl
