#pragma once

#include <memory>

#include "src/common/base/base.h"
#include "src/vizier/services/metadata/metadatapb/service.grpc.pb.h"

namespace pl {
namespace vizier {
namespace agent {
// Forward declare manager to break circular dependence.
class Manager;
}  // namespace agent
namespace funcs {

/**
 * VizierFuncFactoryContext contains Vizier specific information that can be
 * used by the UDTF factories to create new instances.
 *
 */
class VizierFuncFactoryContext : public NotCopyable {
 public:
  using MDSStub = services::metadata::MetadataService::Stub;

  VizierFuncFactoryContext() = default;
  VizierFuncFactoryContext(const agent::Manager* agent_manager,
                           const std::shared_ptr<MDSStub>& mds_stub)
      : agent_manager_(agent_manager), mds_stub_(mds_stub) {}
  virtual ~VizierFuncFactoryContext() = default;

  const agent::Manager* agent_manager() const {
    DCHECK(agent_manager_ != nullptr);
    return agent_manager_;
  }

  std::shared_ptr<MDSStub> mds_stub() const {
    CHECK(mds_stub_ != nullptr);
    return mds_stub_;
  }

 private:
  const agent::Manager* agent_manager_ = nullptr;
  std::shared_ptr<MDSStub> mds_stub_ = nullptr;
};

}  // namespace funcs
}  // namespace vizier
}  // namespace pl
