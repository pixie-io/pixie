#pragma once

#include <memory>

#include "src/common/base/base.h"
#include "src/table_store/table_store.h"
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
  using MDTPStub = services::metadata::MetadataTracepointService::Stub;

  VizierFuncFactoryContext() = default;
  VizierFuncFactoryContext(const agent::Manager* agent_manager,
                           const std::shared_ptr<MDSStub>& mds_stub,
                           const std::shared_ptr<MDTPStub>& mdtp_stub,
                           std::shared_ptr<::pl::table_store::TableStore> table_store,
                           std::function<void(grpc::ClientContext* ctx)> add_grpc_auth)
      : agent_manager_(agent_manager),
        mds_stub_(mds_stub),
        mdtp_stub_(mdtp_stub),
        table_store_(table_store),
        add_auth_to_grpc_context_func_(add_grpc_auth) {}
  virtual ~VizierFuncFactoryContext() = default;

  const agent::Manager* agent_manager() const {
    DCHECK(agent_manager_ != nullptr);
    return agent_manager_;
  }

  std::shared_ptr<MDSStub> mds_stub() const {
    CHECK(mds_stub_ != nullptr);
    return mds_stub_;
  }

  std::shared_ptr<MDTPStub> mdtp_stub() const {
    CHECK(mdtp_stub_ != nullptr);
    return mdtp_stub_;
  }

  ::pl::table_store::TableStore* table_store() const { return table_store_.get(); }

  std::function<void(grpc::ClientContext*)> add_auth_to_grpc_context_func() const {
    CHECK(add_auth_to_grpc_context_func_);
    return add_auth_to_grpc_context_func_;
  }

 private:
  const agent::Manager* agent_manager_ = nullptr;
  std::shared_ptr<MDSStub> mds_stub_ = nullptr;
  std::shared_ptr<MDTPStub> mdtp_stub_ = nullptr;
  std::shared_ptr<::pl::table_store::TableStore> table_store_ = nullptr;
  std::function<void(grpc::ClientContext*)> add_auth_to_grpc_context_func_;
};

}  // namespace funcs
}  // namespace vizier
}  // namespace pl
