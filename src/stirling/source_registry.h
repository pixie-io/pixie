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
#include "src/stirling/bpftrace_connector.h"
#include "src/stirling/proc_stat_connector.h"
#include "src/stirling/source_connector.h"

namespace pl {
namespace stirling {

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
   * @brief Register a source in the registry.
   *
   * @tparam TSourceConnector SourceConnector Class for a data source
   * @param name  Name of the data source
   * @return Status
   */
  template <typename TSourceConnector>
  Status Register(const std::string& name) {
    if (!TSourceConnector::kAvailable) {
      LOG(INFO) << absl::StrFormat(
          "SourceConnector [%s] not registered because it is not available (not "
          "compiled/implemented)",
          name);
      return Status::OK();
    }

    // Create registry element from template
    SourceRegistry::RegistryElement element(TSourceConnector::source_type,
                                            TSourceConnector::Create);
    if (sources_map_.find(name) != sources_map_.end()) {
      return error::AlreadyExists("The data source with name \"$0\" already exists", name);
    }
    sources_map_.insert({name, element});

    return Status::OK();
  }

  template <typename TSourceConnector>
  void RegisterOrDie(const std::string& name) {
    auto status = Register<TSourceConnector>(name);
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

void RegisterAllSources(SourceRegistry* registry);

}  // namespace stirling
}  // namespace pl
