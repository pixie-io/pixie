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

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/stirling/bcc_bpf/http_trace.h"
#include "src/stirling/http_parse.h"
#include "src/stirling/source_connector.h"

DECLARE_string(selected_content_type_substrs);

OBJ_STRVIEW(http_trace_bcc_script, _binary_src_stirling_bcc_bpf_http_trace_c);

namespace pl {
namespace stirling {

class HTTPTraceConnector : public SourceConnector {
 public:
  inline static const std::string_view kBCCScript = http_trace_bcc_script;

  static constexpr SourceType kSourceType = SourceType::kEBPF;
  static constexpr char kName[] = "bcc_http_trace";
  // TODO(yzhao): As of 2019-04-18, all HTTP trace data is written to one table. An option to
  // improve is to have different tables:
  // - A HTTP connection table with fields 1) id (fd+additional_data for uniqueness),
  //   2) {src,dst}_{addr,port}.
  // - A HTTP message table with fields 1) id (same as above), type (req or resp), header, payload.
  inline static const DataElements kElements = {
      DataElement("time_", types::DataType::TIME64NS),
      // tgid is the user space "pid".
      DataElement("tgid", types::DataType::INT64),
      DataElement("pid", types::DataType::INT64),
      DataElement("fd", types::DataType::INT64),
      DataElement("event_type", types::DataType::STRING),
      // TODO(PL-519): Eventually, use the appropriate data type to represent IP addresses, as will
      // be resolved in the Jira issue.
      DataElement("src_addr", types::DataType::STRING),
      DataElement("src_port", types::DataType::INT64),
      DataElement("dst_addr", types::DataType::STRING),
      DataElement("dst_port", types::DataType::INT64),
      DataElement("http_minor_version", types::DataType::INT64),
      DataElement("http_headers", types::DataType::STRING),
      DataElement("http_req_method", types::DataType::STRING),
      DataElement("http_req_path", types::DataType::STRING),
      DataElement("http_resp_status", types::DataType::INT64),
      DataElement("http_resp_message", types::DataType::STRING),
      DataElement("http_resp_body", types::DataType::STRING),
      DataElement("http_resp_latency_ns", types::DataType::INT64),
  };

  static constexpr std::chrono::milliseconds kDefaultSamplingPeriod{100};
  static constexpr std::chrono::milliseconds kDefaultPushPeriod{5000};

  static std::unique_ptr<SourceConnector> Create(const std::string& name) {
    return std::unique_ptr<SourceConnector>(new HTTPTraceConnector(name));
  }
  static void HandleProbeOutput(void* cb_cookie, void* data, int /*data_size*/);
  static void HandleProbeLoss(void* /*cb_cookie*/, uint64_t);

  Status InitImpl() override;
  Status StopImpl() override;
  void TransferDataImpl(types::ColumnWrapperRecordBatch* record_batch) override;

  // 'this' pointer of this class is passed to the callback, and the callback will call this
  // function to get the result argument for processing data items from perf buffer.
  types::ColumnWrapperRecordBatch* GetRecordBatch() const { return record_batch_; }
  void SetRecordBatch(types::ColumnWrapperRecordBatch* record_batch) {
    record_batch_ = record_batch;
  }

 private:
  explicit HTTPTraceConnector(const std::string& source_name)
      : SourceConnector(kSourceType, std::move(source_name), kElements, kDefaultSamplingPeriod,
                        kDefaultPushPeriod) {
    // TODO(oazizi): Is there a better place/time to grab the flags?
    filter_substrs_ = absl::StrSplit(FLAGS_selected_content_type_substrs, ",", absl::SkipEmpty());
  }

  FRIEND_TEST(HandleProbeOutputTest, FilterMessages);
  inline static std::vector<std::string_view> filter_substrs_;

  // Helper functions used by HandleProbeOutput().
  static bool SelectForAppend(const HTTPTraceRecord& record);
  static void AppendToRecordBatch(HTTPTraceRecord record,
                                  types::ColumnWrapperRecordBatch* record_batch);
  static void ConsumeRecord(HTTPTraceRecord record, types::ColumnWrapperRecordBatch* record_batch);

  ebpf::BPF bpf_;
  const int perf_buffer_page_num_ = 8;

  // This holds the target buffer for recording the events captured in http tracing. It roughly
  // works as follows:
  // - The data is sent through perf ring buffer.
  // - The perf ring buffer is opened with a callback that is executed inside kernel.
  // - The callback will write data into this variable.
  // - The callback is triggered when TransferDataImpl() calls BPFTable::poll() and there is items
  // in the buffer.
  // - TransferDataImpl() will assign its input record_batch to this variable, and block during the
  // polling.
  //
  // We need to do this because the callback passed into BPF::open_perf_buffer() is a pure function
  // pointer that cannot be customized to write to a different record batch.
  //
  // TODO(yzhao): A less-possible option: Let the BPF::open_perf_buffer() expose the underlying file
  // descriptor, and let TransferDataImpl() directly poll that file descriptor.
  types::ColumnWrapperRecordBatch* record_batch_ = nullptr;
};

}  // namespace stirling
}  // namespace pl

#endif
