#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <thread>

#include "src/common/base/base.h"
#include "src/common/event/event_class.h"
#include "src/stirling/source_connectors/socket_tracer/protocols/http2/types.h"
#include "src/table_store/table.h"

namespace px {
namespace vizier {
namespace services {
namespace tetragonreader {

class TetragonReader {
 public:
  explicit TetragonReader(table_store::TableStore* table_store);
  ~TetragonReader();

  // Start the background reader thread.
  void Start();

  // Signal the reader thread to stop.
  void Stop();

 private:
  void ReaderThread();
  void Ingest(const std::string& json_line);

  std::shared_ptr<table_store::Table> tetragon_table_;
  table_store::TableStore* table_store_;
  std::atomic<bool> running_ = false;
  std::thread reader_thread_;
};

}  // namespace tetragonreader
}  // namespace services
}  // namespace vizier
}  // namespace px
