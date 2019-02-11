#pragma once

#include <string>
#include <vector>

#include "src/common/base.h"
#include "src/data_collector/info_class_schema.h"

namespace pl {
namespace datacollector {

class InfoClassElement;
class InfoClassSchema;

struct RawDataBuf {
 public:
  RawDataBuf(uint32_t num_records, uint8_t* buf) : num_records(num_records), buf(buf) {}
  uint64_t num_records;
  uint8_t* buf;
};

enum class SourceType : uint8_t { kEBPF = 1, kOpenTracing, kPrometheus };

class SourceConnector : public NotCopyable {
 public:
  SourceConnector() = delete;
  virtual ~SourceConnector() = default;

  Status Init() { return InitImpl(); }
  RawDataBuf GetData() { return GetDataImpl(); }
  Status Stop() { return StopImpl(); }
  Status PopulateSchema(InfoClassSchema* schema);

  SourceType type() { return type_; }
  const std::string& source_name() { return source_name_; }
  const std::vector<InfoClassElement>& elements() { return elements_; }

 protected:
  explicit SourceConnector(SourceType type, const std::string& source_name,
                           const std::vector<InfoClassElement> elements)
      : type_(type), source_name_(source_name), elements_(elements) {}

  virtual Status InitImpl() = 0;
  virtual RawDataBuf GetDataImpl() = 0;
  virtual Status StopImpl() = 0;

 private:
  SourceType type_;
  std::string source_name_;
  std::vector<InfoClassElement> elements_;
};

class EBPFConnector : public SourceConnector {
 public:
  EBPFConnector() = delete;
  explicit EBPFConnector(const std::string& source_name,
                         const std::vector<InfoClassElement> elements,
                         const std::string& kernel_event, const std::string& fn_name,
                         const std::string& bpf_program)
      : SourceConnector(SourceType::kEBPF, source_name, elements),
        kernel_event_(kernel_event),
        fn_name_(fn_name),
        bpf_program_(bpf_program) {}
  virtual ~EBPFConnector() = default;

 protected:
  Status InitImpl() override {
    // TODO(kgandhi): Launch the EBPF program.
    return Status::OK();
  }

  // TODO(kgandhi): Get data records from EBPF program.
  RawDataBuf GetDataImpl() override;

  // TODO(kgandhi): Stop the running EBPF program.
  Status StopImpl() override { return Status::OK(); }

  const std::string& kernel_event() { return kernel_event_; }
  const std::string& fn_name() { return fn_name_; }
  const std::string& bpf_program() { return bpf_program_; }

 private:
  std::string kernel_event_, fn_name_, bpf_program_;
  std::vector<uint8_t> data_buf_;
};

/**
 * @brief Placeholder for Open tracing sources.
 *
 */
class OpenTracingConnector : public SourceConnector {
 public:
  OpenTracingConnector() = delete;
  explicit OpenTracingConnector(const std::string& source_name,
                                const std::vector<InfoClassElement> elements)
      : SourceConnector(SourceType::kEBPF, source_name, elements) {}
  virtual ~OpenTracingConnector() = default;

 protected:
  Status InitImpl() override {
    // TODO(kgandhi): Launch open tracing collection methods.
    return Status::OK();
  }

  // TODO(kgandhi): Get data records from open tracing source.
  RawDataBuf GetDataImpl() override;

  // TODO(kgandhi): Stop the collection of data from the source.
  Status StopImpl() override { return Status::OK(); }

 private:
  std::vector<uint8_t> data_buf_;
};

}  // namespace datacollector
}  // namespace pl
