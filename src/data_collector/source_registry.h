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
#include "src/data_collector/proc_stat_connector.h"
#include "src/data_collector/source_connector.h"

namespace pl {
namespace datacollector {

/**
 * @brief This class maintains a registry of all the sources available to the
 * the data collector.
 * The registry contains a map of source name -> RegistryElements.
 * RegistryElement contains the source type and an std::function using which
 * the data collector can construct a SourceConnector.
 */
class SourceRegistry : public NotCopyable {
 public:
  SourceRegistry() = delete;
  explicit SourceRegistry(const std::string& name) : name_(name) {}
  virtual ~SourceRegistry() = default;

  struct RegistryElement {
    RegistryElement() : type(SourceType::kUnknown), create_source_fn(nullptr) {}
    explicit RegistryElement(SourceType type,
                             std::function<std::unique_ptr<SourceConnector>()> create_source_fn)
        : type(type), create_source_fn(create_source_fn) {}
    SourceType type;
    std::function<std::unique_ptr<SourceConnector>()> create_source_fn;
  };

  const std::string& name() { return name_; }
  const std::unordered_map<std::string, RegistryElement>& sources_map() { return sources_map_; }

  /**
   * @brief Register a source. Each source provides a name and a RegistryElement to register itself.
   * @param name
   * @param element
   * @return Status
   */
  Status Register(const std::string& name, RegistryElement element) {
    if (sources_map_.find(name) != sources_map_.end()) {
      return error::AlreadyExists("The data source with name \"$0\" already exists", name);
    }
    sources_map_.insert({name, element});
    return Status::OK();
  }

  void RegisterOrDie(const std::string& name, RegistryElement element) {
    auto status = Register(name, element);
    PL_CHECK_OK(status);
  }

  StatusOr<RegistryElement> GetRegistryElement(const std::string& name) {
    auto it = sources_map_.find(name);
    if (it == sources_map_.end()) {
      return error::NotFound("The data source with name \"$0\" was not found", name);
    }
    return it->second;
  }

 protected:
  std::string name_;
  std::unordered_map<std::string, RegistryElement> sources_map_;
};

void RegisterSources(SourceRegistry* registry);
void RegisterFakeSources(SourceRegistry* registry);

}  // namespace datacollector
}  // namespace pl
