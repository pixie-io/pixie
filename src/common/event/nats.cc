#include "src/common/event/nats.h"

#include <nats/adapters/libuv.h>
#include <nats/nats.h>

namespace px {
namespace event {

Status NATSConnectorBase::ConnectBase(Dispatcher* base_dispatcher) {
  natsOptions* nats_opts = nullptr;
  natsOptions_Create(&nats_opts);

  LibuvDispatcher* dispatcher = dynamic_cast<LibuvDispatcher*>(base_dispatcher);
  if (dispatcher == nullptr) {
    return error::InvalidArgument("Only libuv based dispatcher is allowed");
  }

  natsLibuv_Init();

  // Tell nats we are using libuv so it uses the correct thread.
  natsLibuv_SetThreadLocalLoop(dispatcher->uv_loop());

  if (tls_config_ != nullptr) {
    natsOptions_SetSecure(nats_opts, true);
    natsOptions_LoadCATrustedCertificates(nats_opts, tls_config_->ca_cert.c_str());
    natsOptions_LoadCertificatesChain(nats_opts, tls_config_->tls_cert.c_str(),
                                      tls_config_->tls_key.c_str());
  }

  auto s = natsOptions_SetEventLoop(nats_opts, dispatcher->uv_loop(), natsLibuv_Attach,
                                    natsLibuv_Read, natsLibuv_Write, natsLibuv_Detach);

  if (s != NATS_OK) {
    return error::Unknown("Failed to set NATS event loop, nats_status=$0", s);
  }

  natsOptions_SetURL(nats_opts, nats_server_.c_str());

  auto nats_status = natsConnection_Connect(&nats_connection_, nats_opts);
  natsOptions_Destroy(nats_opts);
  nats_opts = nullptr;

  if (nats_status != NATS_OK) {
    return error::Unknown("Failed to connect to NATS, nats_status=$0", nats_status);
  }
  return Status::OK();
}

}  // namespace event
}  // namespace px
