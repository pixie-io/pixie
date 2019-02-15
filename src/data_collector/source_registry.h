#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "src/common/error.h"
#include "src/common/status.h"
#include "src/common/statusor.h"
#include "src/common/types/types.pb.h"
#include "src/data_collector/source_connector.h"

namespace pl {
namespace datacollector {

class SourceRegistry {
 public:
  SourceRegistry() = delete;
  explicit SourceRegistry(const std::string& name) : name_(name) {}
  virtual ~SourceRegistry() = default;

  const std::string& name() { return name_; }

  Status Register(const std::string& name, std::unique_ptr<SourceConnector> source_ptr) {
    if (map_.find(name) != map_.end()) {
      return error::AlreadyExists("The data source with name \"$0\" already exists", name);
    }
    map_.insert({name, std::move(source_ptr)});
    return Status::OK();
  }

  void RegisterOrDie(const std::string& name, std::unique_ptr<SourceConnector> source_ptr) {
    auto status = Register(name, std::move(source_ptr));
    PL_CHECK_OK(status);
  }

  StatusOr<SourceConnector*> GetSource(const std::string& name) {
    auto it = map_.find(name);
    if (it == map_.end()) {
      return error::NotFound("The data source with name \"$0\" was not found", name);
    }
    return it->second.get();
  }

 protected:
  std::string name_;
  std::unordered_map<std::string, std::unique_ptr<SourceConnector>> map_;
};

void RegisterSourcesOrDie(SourceRegistry* registry);
void RegisterMetricsSourcesOrDie(SourceRegistry* registry);
std::unique_ptr<SourceConnector> CreateEBPFCPUMetricsSource();

}  // namespace datacollector
}  // namespace pl
