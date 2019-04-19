#pragma once

#ifndef __linux__

#include "src/stirling/source_connector.h"

namespace pl {
namespace stirling {

DUMMY_SOURCE_CONNECTOR(HTTPTraceConnector);

}  // namespace stirling
}  // namespace pl

#else

#include <bcc/BPF.h>

#include <memory>
#include <string>
#include <utility>

#include "src/stirling/source_connector.h"

OBJ_STRVIEW(http_trace_bcc_script, _binary_src_stirling_bcc_bpf_http_trace_c);

namespace pl {
namespace stirling {

class HTTPTraceConnector : public SourceConnector {
 public:
  static constexpr SourceType kSourceType = SourceType::kEBPF;
  static constexpr char kName[] = "bcc_http_trace";
  // TODO(yzhao): As of 2019-04-18, all HTTP trace data is written to one table. An option to
  // improve is to have different tables:
  // - A HTTP connection table with fields 1) id (fd+additional_data for uniqueness),
  //   2) {src,dst}_{addr,port}.
  // - A HTTP message table with fields 1) id (same as above), type (req or resp), header, payload.
  inline static const DataElements kElements = {
      DataElement("time_stamp_ns", types::DataType::TIME64NS),
      // tgid is the user space "pid".
      DataElement("tgid", types::DataType::INT64), DataElement("pid", types::DataType::INT64),
      DataElement("fd", types::DataType::INT64),
      // TODO(yzhao): Add the additional data elements:
      // {src,dst}_{addr,port}, http_{req, resp, payload}
  };

  static std::unique_ptr<SourceConnector> Create(const std::string& name) {
    return std::unique_ptr<SourceConnector>(new HTTPTraceConnector(name));
  }

  Status InitImpl() override;
  Status StopImpl() override;
  void TransferDataImpl(types::ColumnWrapperRecordBatch* record_batch) override;

 private:
  explicit HTTPTraceConnector(const std::string& source_name)
      : SourceConnector(kSourceType, std::move(source_name), kElements) {}

  inline static const std::string_view kBCCScript = http_trace_bcc_script;

  ebpf::BPF bpf_;
  const int perf_buffer_page_num_ = 8;
};

}  // namespace stirling
}  // namespace pl

#endif
