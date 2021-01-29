#pragma once

#include <memory>

#include "src/vizier/services/agent/manager/manager.h"

namespace pl {
namespace vizier {
namespace agent {

using RegistrationHook = std::function<Status(uint32_t asid)>;

// RegistrationHandler handles registration and reregistration.
class RegistrationHandler : public Manager::MessageHandler {
 public:
  RegistrationHandler() = delete;
  RegistrationHandler(pl::event::Dispatcher* dispatcher, Info* agent_info,
                      Manager::VizierNATSConnector* nats_conn,
                      RegistrationHook post_registration_hook,
                      RegistrationHook post_reregistration_hook);
  ~RegistrationHandler() override = default;

  Status HandleMessage(std::unique_ptr<messages::VizierMessage> msg) override;

  void RegisterAgent() { return RegisterAgent(/*reregister*/ false); }
  void ReregisterAgent() { return RegisterAgent(/*reregister*/ true); }

 private:
  void RegisterAgent(bool reregister);
  Status DispatchRegistration();

  RegistrationHook post_registration_hook_;
  RegistrationHook post_reregistration_hook_;

  // Ensures that there is only one registration attempt going on at any given time.
  absl::base_internal::SpinLock registration_lock_;
  // Whether there was ever a successful registration for this agent.
  // Everything after that is treated as a reregistration.
  bool ever_registered_ = false;
  // Whether or not a registration or reregistration is currently in progress.
  bool registration_in_progress_ = false;
  pl::event::TimerUPtr registration_timeout_;
  // The agent waits a random amount of time before sending a register request, to
  // avoid bombarding the metadata service with too many requests upon startup.
  pl::event::TimerUPtr registration_wait_;

  // Timeout for registration ACK.
  static constexpr std::chrono::seconds kRegistrationPeriod{30};
  // Min and max wait times, in ms, to wait before registering.
  static const int32_t kMinWaitTimeMillis = 10;
  static const int32_t kMaxWaitTimeMillis = 60000;
};

}  // namespace agent
}  // namespace vizier
}  // namespace pl
