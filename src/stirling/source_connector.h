#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_format.h"
#include "src/common/base/base.h"
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

class InfoClassElement;
class InfoClassManager;

#define DUMMY_SOURCE_CONNECTOR(NAME)                                       \
  class NAME : public SourceConnector {                                    \
   public:                                                                 \
    static constexpr bool kAvailable = false;                              \
    static constexpr SourceType kSourceType = SourceType::kNotImplemented; \
    static constexpr char kName[] = "dummy";                               \
    inline static const std::vector<DataTableSchema> kElements = {};       \
    static std::unique_ptr<SourceConnector> Create(std::string name) {     \
      PL_UNUSED(name);                                                     \
      return nullptr;                                                      \
    }                                                                      \
  }

enum class SourceType : uint8_t {
  kEBPF = 1,
  kOpenTracing,
  kPrometheus,
  kFile,
  kUnknown,
  kNotImplemented
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

  Status Init() { return InitImpl(); }
  void TransferData(uint32_t table_num, types::ColumnWrapperRecordBatch* record_batch) {
    CHECK_LT(table_num, num_tables())
        << absl::StrFormat("Access to table out of bounds: table_num=%d", table_num);
    return TransferDataImpl(table_num, record_batch);
  }
  Status Stop() { return StopImpl(); }

  SourceType type() const { return type_; }
  const std::string& source_name() const { return source_name_; }

  uint32_t num_tables() const { return table_schemas_.size(); }
  const DataElements& elements(uint32_t table_num) const {
    CHECK_LT(table_num, num_tables())
        << absl::StrFormat("Access to table out of bounds: table_num=%d", table_num);
    return table_schemas_[table_num].elements();
  }
  const std::string& table_name(uint32_t table_num) const {
    CHECK_LT(table_num, num_tables())
        << absl::StrFormat("Access to table out of bounds: table_num=%d", table_num);
    return table_schemas_[table_num].name();
  }

  const std::chrono::milliseconds& default_sampling_period() { return default_sampling_period_; }
  const std::chrono::milliseconds& default_push_period() { return default_push_period_; }

  /**
   * @brief If recording nsecs in your bt file, this function can be used to find the offset for
   * convert the result into realtime.
   */
  uint64_t ClockRealTimeOffset();

 protected:
  explicit SourceConnector(SourceType type, std::string source_name,
                           std::vector<DataTableSchema> table_schemas,
                           std::chrono::milliseconds default_sampling_period,
                           std::chrono::milliseconds default_push_period)
      : type_(type),
        source_name_(std::move(source_name)),
        table_schemas_(std::move(table_schemas)),
        default_sampling_period_(default_sampling_period),
        default_push_period_(default_push_period) {}

  virtual Status InitImpl() = 0;
  virtual void TransferDataImpl(uint32_t table_num,
                                types::ColumnWrapperRecordBatch* record_batch) = 0;
  virtual Status StopImpl() = 0;

  /**
   * @brief Init Helper function: calculates monotonic clock to real time clock offset.
   *
   */
  void InitClockRealTimeOffset();

  uint64_t real_time_offset_;
  static constexpr uint64_t kSecToNanosecFactor = 1000000000;

 private:
  SourceType type_;
  std::string source_name_;
  std::vector<DataTableSchema> table_schemas_;
  std::chrono::milliseconds default_sampling_period_;
  std::chrono::milliseconds default_push_period_;
};

}  // namespace stirling
}  // namespace pl
