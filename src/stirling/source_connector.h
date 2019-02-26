#pragma once

#include <memory>
#include <string>
#include <vector>

#include "absl/strings/str_format.h"
#include "src/common/base.h"
#include "src/common/error.h"
#include "src/common/statusor.h"
#include "src/stirling/info_class_schema.h"

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
    static constexpr SourceType source_type = SourceType::kNotImplemented; \
    static constexpr char kName[] = "dummy";                               \
    inline static const InfoClassSchema kElements = {};                    \
    static std::unique_ptr<SourceConnector> Create() { return nullptr; }   \
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

  SourceConnector() = delete;
  virtual ~SourceConnector() = default;

  Status Init() { return InitImpl(); }
  RawDataBuf GetData() { return GetDataImpl(); }
  Status Stop() { return StopImpl(); }

  Status PopulateSchema(InfoClassManager* mgr) const {
    for (const auto& element : elements_) {
      mgr->Schema().push_back(element);
    }
    return Status::OK();
  }

  SourceType type() { return type_; }
  const std::string& source_name() { return source_name_; }
  const InfoClassSchema& elements() { return elements_; }

 protected:
  explicit SourceConnector(SourceType type, const std::string& source_name,
                           const InfoClassSchema& elements)
      : elements_(elements), type_(type), source_name_(source_name) {}

  virtual Status InitImpl() = 0;
  virtual RawDataBuf GetDataImpl() = 0;
  virtual Status StopImpl() = 0;

 protected:
  InfoClassSchema elements_;

 private:
  SourceType type_;
  std::string source_name_;
};

/**
 * @brief Placeholder for Open tracing sources.
 *
 */
class OpenTracingConnector : public SourceConnector {
 public:
  OpenTracingConnector() = delete;
  static constexpr SourceType source_type = SourceType::kOpenTracing;
  virtual ~OpenTracingConnector() = default;
  static std::unique_ptr<SourceConnector> Create() {
    InfoClassSchema elements = {};
    return std::unique_ptr<SourceConnector>(
        new OpenTracingConnector("open_tracing_connector", elements));
  }

 protected:
  explicit OpenTracingConnector(const std::string& source_name, const InfoClassSchema& elements)
      : SourceConnector(source_type, source_name, elements) {}
  Status InitImpl() override {
    // TODO(kgandhi): Launch open tracing collection methods.
    return Status::OK();
  }

  // TODO(kgandhi): Get data records from open tracing source.
  RawDataBuf GetDataImpl() override {
    uint32_t num_records = 1;
    return RawDataBuf(num_records, data_buf_.data());
  };

  // TODO(kgandhi): Stop the collection of data from the source.
  Status StopImpl() override { return Status::OK(); }

 private:
  std::vector<uint8_t> data_buf_;
};

}  // namespace stirling
}  // namespace pl
