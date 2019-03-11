#pragma once

#include <memory>
#include <string>
#include <vector>

#include "absl/strings/str_format.h"
#include "src/common/base.h"
#include "src/common/error.h"
#include "src/common/statusor.h"
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

#define DUMMY_SOURCE_CONNECTOR(NAME)                                          \
  class NAME : public SourceConnector {                                       \
   public:                                                                    \
    static constexpr bool kAvailable = false;                                 \
    static constexpr SourceType kSourceType = SourceType::kNotImplemented;    \
    static constexpr char kName[] = "dummy";                                  \
    inline static const DataElements kElements = {};                          \
    static std::unique_ptr<SourceConnector> Create(const std::string& name) { \
      PL_UNUSED(name);                                                        \
      return nullptr;                                                         \
    }                                                                         \
  }

struct RawDataBuf {
 public:
  RawDataBuf(uint32_t num_records, uint8_t* buf) : num_records(num_records), buf(buf) {}
  uint64_t num_records;
  uint8_t* buf;
};

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

  inline static const std::chrono::milliseconds kDefaultSamplingPeriod{100};
  inline static const std::chrono::milliseconds kDefaultPushPeriod{1000};

  SourceConnector() = delete;
  virtual ~SourceConnector() = default;

  Status Init() { return InitImpl(); }
  RawDataBuf GetData() { return GetDataImpl(); }
  Status Stop() { return StopImpl(); }

  Status PopulateSchema(InfoClassManager* mgr) const {
    for (const auto& element : elements_) {
      mgr->Schema().emplace_back(InfoClassElement(element));
    }
    return Status::OK();
  }

  SourceType type() const { return type_; }
  const std::string& source_name() const { return source_name_; }
  const DataElements& elements() const { return elements_; }

 protected:
  explicit SourceConnector(SourceType type, const std::string& source_name,
                           const DataElements& elements)
      : elements_(elements), type_(type), source_name_(source_name) {}

  virtual Status InitImpl() = 0;
  virtual RawDataBuf GetDataImpl() = 0;
  virtual Status StopImpl() = 0;

 protected:
  DataElements elements_;

 private:
  SourceType type_;
  std::string source_name_;
};

}  // namespace stirling
}  // namespace pl
