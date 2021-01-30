#pragma once

#include <algorithm>
#include <csignal>
#include <list>
#include <vector>

#include "src/common/base/base.h"
#include "src/common/signal/fatal_handler.h"
#include "src/common/system/system.h"

namespace pl {

// This implementation is based on Envoy's signal handler.

class SignalAction : public NotCopyable {
 public:
  SignalAction()
      : guard_size_(pl::system::Config::GetInstance().PageSize()),
        altstack_size_(std::max(guard_size_ * 4, static_cast<size_t>(MINSIGSTKSZ))) {
    MapAndProtectStackMemory();
    InstallSigHandlers();
  }
  ~SignalAction() {
    RemoveSigHandlers();
    UnmapStackMemory();
  }
  /**
   * The actual signal handler function with prototype matching signal.h
   */
  static void SigHandler(int sig, siginfo_t* info, void* context);

  /**
   * Add this handler to the list of functions which will be called on fatal signal
   */
  static void RegisterFatalErrorHandler(const FatalErrorHandlerInterface& handler);

 private:
  /**
   * Allocate this many bytes on each side of the area used for alt stack.
   *
   * Set to system page size.
   *
   * The memory will be protected from read and write.
   */
  const size_t guard_size_;
  /**
   * Use this many bytes for the alternate signal handling stack.
   *
   * Initialized as a multiple of page size (although signalstack will
   * do alignment as needed).
   *
   * Additionally, two guard pages will be allocated to bookend the usable area.
   */
  const size_t altstack_size_;
  /**
   * Signal handlers will be installed for these signals which have a fatal outcome.
   */
  static constexpr int kFatalSignals[] = {SIGABRT, SIGBUS, SIGFPE, SIGILL, SIGSEGV};
  /**
   * Return the memory size we actually map including two guard pages.
   */
  size_t MapSizeWithGuards() const { return altstack_size_ + guard_size_ * 2; }
  /**
   * Install all signal handlers and setup signal handling stack.
   */
  void InstallSigHandlers();

  /**
   * Use mmap to map anonymous memory for the alternative stack.
   *
   * GUARD_SIZE on either end of the memory will be marked PROT_NONE, protected
   * from all access.
   */
  void MapAndProtectStackMemory();
  /**
   * Unmap alternative stack memory.
   */
  void UnmapStackMemory();

  void RemoveSigHandlers();

  char* altstack_{};
  std::array<struct sigaction, sizeof(kFatalSignals) / sizeof(int)> previous_handlers_;
  stack_t previous_altstack_;
  std::list<const FatalErrorHandlerInterface*> fatal_error_handlers_;
};

}  // namespace pl
