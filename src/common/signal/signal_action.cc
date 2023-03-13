/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "src/common/signal/signal_action.h"

#include <sys/mman.h>

#include <string>

#include <absl/base/attributes.h>
#include <absl/synchronization/mutex.h>
#include "threadstacks/signal_handler.h"

namespace px {

using ::threadstacks::StackTraceCollector;

// ErrLog without buffering.
static void ErrLog(const char* msg) {
  int n = write(STDERR_FILENO, msg, strlen(msg));
  PX_UNUSED(n);
}

ABSL_CONST_INIT static absl::Mutex failure_mutex(absl::kConstInit);
// Since we can't grab the failure mutex on fatal error (snagging locks under
// fatal crash causing potential deadlocks) access the handler list as an atomic
// operation, to minimize the chance that one thread is operating on the list
// while the crash handler is attempting to access it.
// This basically makes edits to the list thread-safe - if one thread is
// modifying the list rather than crashing in the crash handler due to accessing
// the list in a non-thread-safe manner, it simply won't log crash traces.
using FailureFunctionList = std::list<const FatalErrorHandlerInterface*>;
ABSL_CONST_INIT std::atomic<FailureFunctionList*> fatal_error_handlers{nullptr};

void SignalAction::RegisterFatalErrorHandler(const FatalErrorHandlerInterface& handler) {
  absl::MutexLock l(&failure_mutex);
  FailureFunctionList* list = fatal_error_handlers.exchange(nullptr, std::memory_order_relaxed);
  if (list == nullptr) {
    list = new FailureFunctionList;
  }
  list->push_back(&handler);
  fatal_error_handlers.store(list, std::memory_order_release);
}

void SignalAction::SigHandler(int sig, siginfo_t* info, void* ucontext) {
  // Don't dump stack trace for term signal.
  if (sig != SIGTERM) {
    LOG(ERROR) << absl::StrFormat("Caught %s, suspect faulting address %p. Trace:", strsignal(sig),
                                  info->si_addr);

    // First print out the current threads stack in as safe of a way as possible.
    threadstacks::BackwardsTrace tracer;
    ErrLog("**************************\n");
    tracer.Capture(ucontext);
    tracer.stack().PrettyPrint([](const char* str) { ErrLog(str); });
    ErrLog("**************************\n");

    // Try to get stack of other threads. This might cause a crash because it's not
    // async-signal-safe.
    std::string error;
    StackTraceCollector collector;
    auto results = collector.Collect(&error);
    if (results.empty()) {
      std::cerr << "StackTrace collection failed: " << error << std::endl;
    } else {
      const auto& trace = StackTraceCollector::ToPrettyString(results);
      ErrLog(trace.c_str());
    }
  }
  FailureFunctionList* list = fatal_error_handlers.exchange(nullptr, std::memory_order_relaxed);
  if (list) {
    // Finally after logging the stack trace, call any registered crash handlers.
    for (const auto* handler : *list) {
      handler->OnFatalError();
    }
  }

  signal(sig, SIG_DFL);
  raise(sig);
}

void SignalAction::InstallSigHandlers() {
  // Install handlers for stack tracing all threads.
  threadstacks::StackTraceSignal::InstallInternalHandler();
  threadstacks::StackTraceSignal::InstallExternalHandler();

  stack_t stack;
  stack.ss_sp = altstack_ + guard_size_;  // Guard page at one end ...
  stack.ss_size = altstack_size_;         // ... guard page at the other.
  stack.ss_flags = 0;

  CHECK_EQ(sigaltstack(&stack, &previous_altstack_), 0);

  int idx = 0;
  for (const auto& sig : kFatalSignals) {
    struct sigaction saction;
    std::memset(&saction, 0, sizeof(saction));
    sigemptyset(&saction.sa_mask);
    // Reset the signal handler, so crash within the handler will kill the process.
    saction.sa_flags = (SA_SIGINFO | SA_ONSTACK | SA_RESETHAND | SA_NODEFER);
    saction.sa_sigaction = SigHandler;
    auto* handler = &previous_handlers_[idx++];
    CHECK_EQ(sigaction(sig, &saction, handler), 0);
  }
}

void SignalAction::RemoveSigHandlers() {
#if defined(__APPLE__)
  // ss_flags contains SS_DISABLE, but Darwin still checks the size, contrary to the man page.
  if (previous_altstack_.ss_size < MINSIGSTKSZ) {
    previous_altstack_.ss_size = MINSIGSTKSZ;
  }
#endif
  CHECK_EQ(sigaltstack(&previous_altstack_, nullptr), 0);

  int idx = 0;
  for (const auto& sig : kFatalSignals) {
    auto* handler = &previous_handlers_[idx++];
    CHECK_EQ(sigaction(sig, handler, nullptr), 0);
  }
}

#if defined(__APPLE__) && !defined(MAP_STACK)
#define MAP_STACK (0)
#endif

void SignalAction::MapAndProtectStackMemory() {
  // Per docs MAP_STACK doesn't actually do anything today but provides a
  // library hint that might be used in the future.
  altstack_ = static_cast<char*>(mmap(nullptr, MapSizeWithGuards(), PROT_READ | PROT_WRITE,
                                      MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, -1, 0));
  CHECK(altstack_);
  CHECK_EQ(mprotect(altstack_, guard_size_, PROT_NONE), 0);
  CHECK_EQ(mprotect(altstack_ + guard_size_ + altstack_size_, guard_size_, PROT_NONE), 0);
}

void SignalAction::UnmapStackMemory() { munmap(altstack_, MapSizeWithGuards()); }

}  // namespace px
