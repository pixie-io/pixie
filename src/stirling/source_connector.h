#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/common/base/base.h"
#include "src/common/system/system.h"
#include "src/shared/metadata/metadata_state.h"
#include "src/shared/types/types.h"
#include "src/stirling/data_table.h"
#include "src/stirling/info_class_manager.h"

/**
 * These are the steps to follow to add a new data source connector.
 * 1. If required, create a new SourceConnector class.
 * 2. Add a new Create function with the following signature:
 *    static std::unique_ptr<SourceConnector> Create().
 *    In this function create an InfoClassSchema (vector of DataElement)
 * 3. Register the data source in the appropriate registry.
 */

namespace pl {
namespace stirling {

class InfoClassManager;

#define DUMMY_SOURCE_CONNECTOR(NAME)                                        \
  class NAME : public SourceConnector {                                     \
   public:                                                                  \
    static constexpr bool kAvailable = false;                               \
    static constexpr std::array<DataTableSchema, 0> kTables = {};           \
    static std::unique_ptr<SourceConnector> Create(std::string_view name) { \
      PL_UNUSED(name);                                                      \
      return nullptr;                                                       \
    }                                                                       \
  }

/**
 * ConnectorContext is the information passed on every Transfer call to source connectors.
 */
class ConnectorContext {
 public:
  ConnectorContext() = default;
  ~ConnectorContext() = default;

  /**
   * ConntectoContext with metadata state.
   * @param agent_metadata_state A read-only snapshot view of the metadata state. This state
   * should not be held onto for extended periods of time.
   */
  explicit ConnectorContext(std::shared_ptr<const md::AgentMetadataState> agent_metadata_state)
      : agent_metadata_state_(std::move(agent_metadata_state)) {}

  /**
   * Get an unowned pointer to the internal agent metadata state.
   * @return either the agent metadata state or null ptr if the state is not valid.
   */
  const md::AgentMetadataState* AgentMetadataState() const {
    if (!agent_metadata_state_) {
      return nullptr;
    }
    return agent_metadata_state_.get();
  }

  uint32_t GetASID() const {
    const auto* md = AgentMetadataState();
    if (md == nullptr) {
      return 0;
    }
    return md->asid();
  }

  absl::flat_hash_set<md::UPID> GetMdsUpids() const {
    auto* md = AgentMetadataState();
    if (md == nullptr) {
      return {};
    }
    absl::flat_hash_set<md::UPID> upids;
    for (const auto& [upid, pid_info] : md->pids_by_upid()) {
      upids.insert(upid);
    }
    return upids;
  }

 private:
  std::shared_ptr<const md::AgentMetadataState> agent_metadata_state_;
};

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
  void TransferData(ConnectorContext* ctx, uint32_t table_num, DataTable* data_table);

  /**
   * @brief Stops the source connector and releases any acquired resources.
   * May only be called after a successful Init().
   *
   * @return Status of whether stop was successful.
   */
  Status Stop();

  const std::string& source_name() const { return source_name_; }

  uint32_t num_tables() const { return table_schemas_.size(); }

  const DataTableSchema& TableSchema(uint32_t table_num) const {
    DCHECK_LT(table_num, num_tables())
        << absl::Substitute("Access to table out of bounds: table_num=$0", table_num);
    return table_schemas_[table_num];
  }

  static constexpr uint32_t TableNum(ArrayView<DataTableSchema> tables,
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
  uint64_t ClockRealTimeOffset() const { return sysconfig_.ClockRealTimeOffset(); }

 protected:
  explicit SourceConnector(std::string_view source_name,
                           const ArrayView<DataTableSchema>& table_schemas,
                           std::chrono::milliseconds default_sampling_period,
                           std::chrono::milliseconds default_push_period)
      : source_name_(source_name),
        table_schemas_(table_schemas),
        default_sampling_period_(default_sampling_period),
        default_push_period_(default_push_period) {}

  virtual Status InitImpl() = 0;
  virtual void TransferDataImpl(ConnectorContext* ctx, uint32_t table_num,
                                DataTable* data_table) = 0;
  virtual Status StopImpl() = 0;

 protected:
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

 protected:
  /**
   * Track state of connector. A connector's lifetime typically progresses sequentially
   * from kUninitialized -> kActive -> KStopped.
   *
   * kErrors is a special state to track a bad state.
   */
  enum class State { kUninitialized, kActive, kStopped, kErrors };

  // Sub-classes are allowed to inspect state.
  State state() const { return state_; }

  const system::Config& sysconfig_ = system::Config::GetInstance();

 private:
  std::atomic<State> state_ = State::kUninitialized;

  const std::string source_name_;
  const ArrayView<DataTableSchema> table_schemas_;
  const std::chrono::milliseconds default_sampling_period_;
  const std::chrono::milliseconds default_push_period_;
};

}  // namespace stirling
}  // namespace pl
