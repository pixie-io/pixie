#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "src/common/base/base.h"
#include "src/shared/types/proto/types.pb.h"
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
  SourceRegistry() = default;
  virtual ~SourceRegistry() = default;

  struct RegistryElement {
    RegistryElement() : create_source_fn(nullptr), schema(ConstVectorView<DataTableSchema>()) {}
    explicit RegistryElement(
        SourceType type,
        std::function<std::unique_ptr<SourceConnector>(const std::string&)> create_source_fn,
        const ConstVectorView<DataTableSchema>& schema)
        : type(type), create_source_fn(std::move(create_source_fn)), schema(schema) {}
    SourceType type{SourceType::kUnknown};
    std::function<std::unique_ptr<SourceConnector>(const std::string&)> create_source_fn;
    ConstVectorView<DataTableSchema> schema;
  };

  const auto& sources() { return sources_map_; }

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
    SourceRegistry::RegistryElement element(TSourceConnector::kSourceType, TSourceConnector::Create,
                                            TSourceConnector::kTables);
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

  StatusOr<RegistryElement> GetRegistryElement(const std::string& name) const {
    auto it = sources_map_.find(name);
    if (it == sources_map_.end()) {
      return error::NotFound("The data source with name \"$0\" was not found", name);
    }
    return it->second;
  }

 protected:
  std::unordered_map<std::string, RegistryElement> sources_map_;
};

void RegisterAllSources(SourceRegistry* registry);

}  // namespace stirling
}  // namespace pl
