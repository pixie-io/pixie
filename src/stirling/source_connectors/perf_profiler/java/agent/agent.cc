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

// This file is compiled into agent.so, a shared library that will be injected into the target
// Java process. Once injected, it creates a symbol log file into which it writes every
// symbol compiled by the JVM along with the symbol address and code size. This symbol file
// is then used by the Stirling data collector inside of the PEM (Pixie Edge Module) to
// populate Java symbols that cannot be found by inspecting binaries (i.e. cannot be found
// by the "normal" means used to find symbols in compiled binaries from C, C++, Go, and Rust).

#include <jvmti.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <mutex>

// NOLINTNEXTLINE: build/include_subdir
#include "raw_symbol_update.h"

namespace {
constexpr bool kUsingTxtLogFile = false;
constexpr bool kUsingBinLogFile = true;

std::mutex g_mtx;
bool g_callbacks_attached = false;
FILE* g_log_file_ptr = nullptr;
FILE* g_bin_file_ptr = nullptr;

}  // namespace

void LogF(const char* format, ...) {
  if (g_log_file_ptr) {
    va_list ap;

    va_start(ap, format);
    vfprintf(g_log_file_ptr, format, ap);
    fprintf(g_log_file_ptr, "\n");
    fflush(g_log_file_ptr);
    va_end(ap);
  }
}

void LogJvmtiError(jvmtiEnv* jvmti, const jvmtiError err_num, const char* help_msg_or_null) {
  if (err_num != JVMTI_ERROR_NONE) {
    const char* const default_err_name_str = "unknown";
    const char* const default_help_message = "";

    char* err_name_or_null;
    jvmti->GetErrorName(err_num, &err_name_or_null);

    const bool err_name_valid = err_name_or_null != nullptr;
    const bool help_msg_valid = help_msg_or_null != nullptr;

    const char* const err_name_str = err_name_valid ? err_name_or_null : default_err_name_str;
    const char* const help_message = help_msg_valid ? help_msg_or_null : default_help_message;

    LogF("[error][%d] %s: %s.", err_num, err_name_str, help_message);
  }
}

void Deallocate(jvmtiEnv* jvmti, void* p) {
  const jvmtiError error = jvmti->Deallocate((unsigned char*)p);
  if (error != JVMTI_ERROR_NONE) {
    LogJvmtiError(jvmti, error, "Deallocate() failure.");
  }
}

template <typename T>
void FWriteRetryOnErr(FILE* f, T* p, const size_t n_total) {
  constexpr size_t kElementSizeInBytes = 1;
  constexpr uint32_t kMaxRetries = 5;

  uint32_t num_retries = 0;
  size_t n_written = 0;
  const char* write_ptr = reinterpret_cast<const char*>(p);

  while (n_written < n_total) {
    if (num_retries > kMaxRetries) {
      LogF("[error] FWriteRetryOnErr() exceeded kMaxRetries: %d.", kMaxRetries);
      break;
    }

    n_written += fwrite(write_ptr, kElementSizeInBytes, n_total - n_written, f);
    write_ptr += n_written;
    ++num_retries;
  }
}

void WriteSymbol(const uint64_t addr, const uint32_t code_size, const bool method_unload,
                 const char* const symbol_ptr, const char* const fn_sig_ptr,
                 const char* const class_sig_ptr) {
  const bool fn_sig_valid = fn_sig_ptr != nullptr;
  const bool symbol_valid = symbol_ptr != nullptr;
  const bool class_sig_valid = class_sig_ptr != nullptr;

  const char nullchar = '\0';
  const char* const fn_sig = fn_sig_valid ? fn_sig_ptr : &nullchar;
  const char* const symbol = symbol_valid ? symbol_ptr : &nullchar;
  const char* const class_sig = class_sig_valid ? class_sig_ptr : &nullchar;

  px::stirling::java::RawSymbolUpdate symbol_metadata = {.addr = addr,
                                                         .code_size = code_size,
                                                         .symbol_size = 1 + strlen(symbol),
                                                         .fn_sig_size = 1 + strlen(fn_sig),
                                                         .class_sig_size = 1 + strlen(class_sig),
                                                         .method_unload = method_unload};

  g_mtx.lock();
  LogF("WriteSymbol|0x%016llx|%u|%s|%s|%s", addr, code_size, symbol, fn_sig, class_sig);
  if (g_bin_file_ptr != nullptr) {
    FWriteRetryOnErr(g_bin_file_ptr, &symbol_metadata, sizeof(symbol_metadata));
    FWriteRetryOnErr(g_bin_file_ptr, symbol, symbol_metadata.symbol_size);
    FWriteRetryOnErr(g_bin_file_ptr, fn_sig, symbol_metadata.fn_sig_size);
    FWriteRetryOnErr(g_bin_file_ptr, class_sig, symbol_metadata.class_sig_size);
    fflush(g_bin_file_ptr);
  }
  g_mtx.unlock();
}

void JNICALL CompiledMethodUnload(jvmtiEnv* jvmti, jmethodID method, const void* addr) {
  const uint64_t code_addr = uint64_t(addr);
  const uint32_t code_size = 0;
  const bool method_unload = true;
  const char* const symbol_ptr = nullptr;
  const char* const fn_sig_ptr = nullptr;
  const char* const class_sig_ptr = nullptr;
  WriteSymbol(code_addr, code_size, method_unload, symbol_ptr, fn_sig_ptr, class_sig_ptr);
}

void JNICALL DynamicCodeGenerated(jvmtiEnv* jvmti, const char* symbol, const void* addr,
                                  jint code_size) {
  const uint64_t code_addr = uint64_t(addr);
  const bool method_unload = false;
  const char* const fn_sig_ptr = nullptr;
  const char* const class_sig_ptr = nullptr;
  WriteSymbol(code_addr, code_size, method_unload, symbol, fn_sig_ptr, class_sig_ptr);
}

void JNICALL CompiledMethodLoad(jvmtiEnv* jvmti, jmethodID method, jint code_size, const void* addr,
                                jint map_length, const jvmtiAddrLocationMap* map,
                                const void* compile_info) {
  jvmtiError error;
  jclass java_class;
  char* symbol_ptr;
  char* fn_sig_ptr;
  char* class_sig_ptr;

  error = jvmti->GetMethodName(method, &symbol_ptr, &fn_sig_ptr, nullptr);
  if (error != JVMTI_ERROR_NONE) {
    LogJvmtiError(jvmti, error, "GetMethodName() failure.");
    return;
  }

  error = jvmti->GetMethodDeclaringClass(method, &java_class);
  if (error != JVMTI_ERROR_NONE) {
    LogJvmtiError(jvmti, error, "GetMethodDeclaringClass() failure.");
    return;
  }

  error = jvmti->GetClassSignature(java_class, &class_sig_ptr, nullptr);
  if (error != JVMTI_ERROR_NONE) {
    LogJvmtiError(jvmti, error, "GetClassSignature() failure.");
    return;
  }

  const bool method_unload = false;
  const uint64_t code_addr = (uint64_t)addr;
  WriteSymbol(code_addr, code_size, method_unload, symbol_ptr, fn_sig_ptr, class_sig_ptr);

  if (symbol_ptr) {
    Deallocate(jvmti, symbol_ptr);
  }
  if (fn_sig_ptr) {
    Deallocate(jvmti, fn_sig_ptr);
  }
  if (class_sig_ptr) {
    Deallocate(jvmti, class_sig_ptr);
  }
}

jint SetNotificationModes(jvmtiEnv* jvmti) {
  jvmtiError error;
  error = jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_COMPILED_METHOD_LOAD, nullptr);
  if (error != JVMTI_ERROR_NONE) {
    LogJvmtiError(jvmti, error, "Unable to set notification mode for CompiledMethodLoad.");
    return JNI_ERR;
  }

  error =
      jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_COMPILED_METHOD_UNLOAD, nullptr);
  if (error != JVMTI_ERROR_NONE) {
    LogJvmtiError(jvmti, error, "Unable to set notification mode for CompiledMethodUnload.");
    return JNI_ERR;
  }

  error =
      jvmti->SetEventNotificationMode(JVMTI_ENABLE, JVMTI_EVENT_DYNAMIC_CODE_GENERATED, nullptr);
  if (error != JVMTI_ERROR_NONE) {
    LogJvmtiError(jvmti, error, "Unable to set notification mode for DynamicCodeGenerated.");
    return JNI_ERR;
  }
  return JNI_OK;
}

jint AddJVMTICapabilities(jvmtiEnv* jvmti) {
  jvmtiError error;

  jvmtiCapabilities capabilities;
  memset(&capabilities, 0, sizeof(jvmtiCapabilities));

  capabilities.can_get_source_file_name = 1;
  capabilities.can_get_line_numbers = 1;
  capabilities.can_generate_compiled_method_load_events = 1;

  error = jvmti->AddCapabilities(&capabilities);
  if (error != JVMTI_ERROR_NONE) {
    LogJvmtiError(jvmti, error, "Unable to get necessary JVMTI capabilities.");
    return JNI_ERR;
  }
  return JNI_OK;
}

jint SetCallbackFunctions(jvmtiEnv* jvmti) {
  jvmtiError error;
  jvmtiEventCallbacks callbacks;
  memset(&callbacks, 0, sizeof(jvmtiEventCallbacks));

  callbacks.CompiledMethodLoad = &CompiledMethodLoad;
  callbacks.CompiledMethodUnload = &CompiledMethodUnload;
  callbacks.DynamicCodeGenerated = &DynamicCodeGenerated;

  error = jvmti->SetEventCallbacks(&callbacks, sizeof(jvmtiEventCallbacks));
  if (error != JVMTI_ERROR_NONE) {
    LogJvmtiError(jvmti, error, "Unable to attach CompiledMethodLoad callback.");
    return JNI_ERR;
  }
  g_callbacks_attached = true;
  return JNI_OK;
}

jint ReplayCallbacks(jvmtiEnv* jvmti) {
  jvmtiError error;
  jvmtiPhase phase;

  error = jvmti->GetPhase(&phase);
  if (phase != JVMTI_PHASE_LIVE) {
    LogF("Skipping ReplayCallbacks(), not in live phase.");
    return JNI_OK;
  }

  LogF("Replaying JIT code compile events.");
  error = jvmti->GenerateEvents(JVMTI_EVENT_COMPILED_METHOD_LOAD);
  if (error != JVMTI_ERROR_NONE) {
    LogJvmtiError(jvmti, error, "GenerateEvents(JVMTI_EVENT_COMPILED_METHOD_LOAD).");
    return JNI_ERR;
  }

  LogF("Replaying dynamic code gen. events.");
  error = jvmti->GenerateEvents(JVMTI_EVENT_DYNAMIC_CODE_GENERATED);
  if (error != JVMTI_ERROR_NONE) {
    LogJvmtiError(jvmti, error, "GenerateEvents(JVMTI_EVENT_DYNAMIC_CODE_GENERATED).");
    return JNI_ERR;
  }
  return JNI_OK;
}

FILE* FOpenLogFile(const std::string& file_path) {
  FILE* f = fopen(file_path.c_str(), "w");
  if (f == nullptr) {
    LogF("[error] FOpenLogFile() Unable to open: %s.", file_path.c_str());
  }
  return f;
}

jint OpenLogFiles(const char* options) {
  if (options == nullptr) {
    return JNI_ERR;
  }

  std::string tmp_path_pfx(options);
  g_log_file_ptr = nullptr;
  g_bin_file_ptr = nullptr;

  if (kUsingTxtLogFile) {
    // TODO(jps): remove the txt based log file once we finalize java symbolization.
    g_log_file_ptr = FOpenLogFile(tmp_path_pfx + ".log");
    if (g_log_file_ptr == nullptr) {
      return JNI_ERR;
    }
  }
  if (kUsingBinLogFile) {
    g_bin_file_ptr = FOpenLogFile(tmp_path_pfx + ".bin");
    if (g_bin_file_ptr == nullptr) {
      return JNI_ERR;
    }
  }
  LogF("OpenLogFiles(), log file path pfx: %s.", options);
  return JNI_OK;
}

jint GetJVMTIEnv(JavaVM* jvm, jvmtiEnv** jvmti) {
  if (jvm == nullptr || jvmti == nullptr) {
    return JNI_ERR;
  }

  jint error = jvm->GetEnv(reinterpret_cast<void**>(jvmti), JVMTI_VERSION_1_0);
  if (error != JNI_OK || *jvmti == nullptr) {
    LogF("[error] Unable to access JVMTI.");
    error = JNI_ERR;
  }
  return error;
}

extern "C" {
JNIEXPORT uint64_t PixieJavaAgentTestFn() {
  // This function is exported solely for the purpose of verifying that
  // dlopen + dlsym works as expected.
  // TODO(jps): move the value "42" into kExpectedJavaAgentTestFnReturnVal in a shared header.
  return 42;
}

JNIEXPORT jint JNICALL Agent_OnAttach(JavaVM* jvm, char* options, void* /*reserved*/) {
  if (g_callbacks_attached) {
    LogF("Agent_OnAttach().");
    LogF("pem restart detected, skipping callback attach.");
    LogF("return JNI_OK.");
    return JNI_OK;
  }

  jint error = JNI_OK;
  static jvmtiEnv* jvmti = nullptr;

  error = error == JNI_OK ? OpenLogFiles(options) : error;
  error = error == JNI_OK ? GetJVMTIEnv(jvm, &jvmti) : error;
  error = error == JNI_OK ? AddJVMTICapabilities(jvmti) : error;
  error = error == JNI_OK ? SetNotificationModes(jvmti) : error;
  error = error == JNI_OK ? SetCallbackFunctions(jvmti) : error;
  error = error == JNI_OK ? ReplayCallbacks(jvmti) : error;

  const char* const return_msg = error == JNI_OK ? "return JNI_OK." : "return JNI_ERR.";
  LogF(return_msg);
  return error;
}

JNIEXPORT jint JNICALL Agent_OnLoad(JavaVM* jvm, char* options, void* /*reserved*/) {
  return Agent_OnAttach(jvm, options, nullptr);
}
}
