#pragma once

namespace px {

/**
 * FatalErrorHandler is the interface to use for error handling
 * for fatal errors.
 *
 * The behavior may not be well defined if the functions are not
 * async-signal-safe.
 */
class FatalErrorHandlerInterface {
 public:
  virtual ~FatalErrorHandlerInterface() = default;
  virtual void OnFatalError() const = 0;
};

}  // namespace px
