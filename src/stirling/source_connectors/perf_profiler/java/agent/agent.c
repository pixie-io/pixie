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

// LINT_C_FILE: Do not remove this line. It ensures cpplint treats this as a C file.

#include <jvmti.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "src/stirling/source_connectors/perf_profiler/java/agent/agent_hash.h"
#include "src/stirling/source_connectors/perf_profiler/java/agent/raw_symbol_update.h"

#define PX_JVMTI_AGENT_RETURN_IF_ERROR(err)                                                    \
  if (err != JNI_OK) {                                                                         \
    LogF("Pixie symbolization agent startup sequence failed, %s, error code: %d.", #err, err); \
    return err;                                                                                \
  }

static bool kUsingTxtLogFile = false;
static bool kUsingBinLogFile = true;

pthread_mutex_t g_mtx = PTHREAD_MUTEX_INITIALIZER;
bool g_callbacks_attached = false;
FILE* g_log_file_ptr = NULL;
FILE* g_bin_file_ptr = NULL;

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
    (*jvmti)->GetErrorName(jvmti, err_num, &err_name_or_null);

    const bool err_name_valid = err_name_or_null != NULL;
    const bool help_msg_valid = help_msg_or_null != NULL;

    const char* const err_name_str = err_name_valid ? err_name_or_null : default_err_name_str;
    const char* const help_message = help_msg_valid ? help_msg_or_null : default_help_message;

    LogF("[error][%d] %s: %s.", err_num, err_name_str, help_message);
  }
}

void Deallocate(jvmtiEnv* jvmti, void* p) {
  const jvmtiError error = (*jvmti)->Deallocate(jvmti, (unsigned char*)p);
  if (error != JVMTI_ERROR_NONE) {
    LogJvmtiError(jvmti, error, "Deallocate() failure.");
  }
}

void FWriteRetryOnErr(FILE* f, const char* write_ptr, const size_t n_total) {
  static const size_t kElementSizeInBytes = 1;
  static const uint32_t kMaxRetries = 5;

  uint32_t num_retries = 0;
  size_t n_written = 0;
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
  const bool fn_sig_valid = fn_sig_ptr != NULL;
  const bool symbol_valid = symbol_ptr != NULL;
  const bool class_sig_valid = class_sig_ptr != NULL;

  const char nullchar = '\0';
  const char* const fn_sig = fn_sig_valid ? fn_sig_ptr : &nullchar;
  const char* const symbol = symbol_valid ? symbol_ptr : &nullchar;
  const char* const class_sig = class_sig_valid ? class_sig_ptr : &nullchar;

  struct RawSymbolUpdate symbol_metadata = {.addr = addr,
                                            .code_size = code_size,
                                            .symbol_size = 1 + strlen(symbol),
                                            .fn_sig_size = 1 + strlen(fn_sig),
                                            .class_sig_size = 1 + strlen(class_sig),
                                            .method_unload = method_unload};

  pthread_mutex_lock(&g_mtx);
  if (method_unload) {
    LogF("WriteSymbol|0x%016llx|unload", addr);
  } else {
    LogF("WriteSymbol|0x%016llx|%u|%s|%s|%s", addr, code_size, symbol, fn_sig, class_sig);
  }
  if (g_bin_file_ptr != NULL) {
    FWriteRetryOnErr(g_bin_file_ptr, (const char*)&symbol_metadata, sizeof(symbol_metadata));
    FWriteRetryOnErr(g_bin_file_ptr, symbol, symbol_metadata.symbol_size);
    FWriteRetryOnErr(g_bin_file_ptr, fn_sig, symbol_metadata.fn_sig_size);
    FWriteRetryOnErr(g_bin_file_ptr, class_sig, symbol_metadata.class_sig_size);
    fflush(g_bin_file_ptr);
  }
  pthread_mutex_unlock(&g_mtx);
}

void JNICALL CompiledMethodUnload(jvmtiEnv* jvmti, jmethodID method, const void* addr) {
  const uint64_t code_addr = (uint64_t)addr;
  const uint32_t code_size = 0;
  const bool method_unload = true;
  const char* const symbol_ptr = NULL;
  const char* const fn_sig_ptr = NULL;
  const char* const class_sig_ptr = NULL;
  WriteSymbol(code_addr, code_size, method_unload, symbol_ptr, fn_sig_ptr, class_sig_ptr);
}

void JNICALL DynamicCodeGenerated(jvmtiEnv* jvmti, const char* symbol, const void* addr,
                                  jint code_size) {
  const uint64_t code_addr = (uint64_t)addr;
  const bool method_unload = false;
  const char* const fn_sig_ptr = NULL;
  const char* const class_sig_ptr = NULL;
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

  error = (*jvmti)->GetMethodName(jvmti, method, &symbol_ptr, &fn_sig_ptr, NULL);
  if (error != JVMTI_ERROR_NONE) {
    LogJvmtiError(jvmti, error, "GetMethodName() failure.");
    return;
  }

  error = (*jvmti)->GetMethodDeclaringClass(jvmti, method, &java_class);
  if (error != JVMTI_ERROR_NONE) {
    LogJvmtiError(jvmti, error, "GetMethodDeclaringClass() failure.");
    return;
  }

  error = (*jvmti)->GetClassSignature(jvmti, java_class, &class_sig_ptr, NULL);
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
  error = (*jvmti)->SetEventNotificationMode(jvmti, JVMTI_ENABLE, JVMTI_EVENT_COMPILED_METHOD_LOAD,
                                             NULL);
  if (error != JVMTI_ERROR_NONE) {
    LogJvmtiError(jvmti, error, "Unable to set notification mode for CompiledMethodLoad.");
    return JNI_ERR;
  }

  error = (*jvmti)->SetEventNotificationMode(jvmti, JVMTI_ENABLE,
                                             JVMTI_EVENT_COMPILED_METHOD_UNLOAD, NULL);
  if (error != JVMTI_ERROR_NONE) {
    LogJvmtiError(jvmti, error, "Unable to set notification mode for CompiledMethodUnload.");
    return JNI_ERR;
  }

  error = (*jvmti)->SetEventNotificationMode(jvmti, JVMTI_ENABLE,
                                             JVMTI_EVENT_DYNAMIC_CODE_GENERATED, NULL);
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

  error = (*jvmti)->AddCapabilities(jvmti, &capabilities);
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

  error = (*jvmti)->SetEventCallbacks(jvmti, &callbacks, sizeof(jvmtiEventCallbacks));
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

  error = (*jvmti)->GetPhase(jvmti, &phase);
  if (error != JVMTI_ERROR_NONE) {
    LogJvmtiError(jvmti, error, "ReplayCallbacks(): GetPhase() error.");
    return JNI_ERR;
  }

  if (phase != JVMTI_PHASE_LIVE) {
    LogF("Skipping ReplayCallbacks(), not in live phase.");
    return JNI_OK;
  }

  LogF("Replaying JIT code compile events.");
  error = (*jvmti)->GenerateEvents(jvmti, JVMTI_EVENT_COMPILED_METHOD_LOAD);
  if (error != JVMTI_ERROR_NONE) {
    LogJvmtiError(jvmti, error, "GenerateEvents(JVMTI_EVENT_COMPILED_METHOD_LOAD).");
    return JNI_ERR;
  }

  LogF("Replaying dynamic code gen. events.");
  error = (*jvmti)->GenerateEvents(jvmti, JVMTI_EVENT_DYNAMIC_CODE_GENERATED);
  if (error != JVMTI_ERROR_NONE) {
    LogJvmtiError(jvmti, error, "GenerateEvents(JVMTI_EVENT_DYNAMIC_CODE_GENERATED).");
    return JNI_ERR;
  }
  return JNI_OK;
}

jint CheckForOnLoadOrLivePhase(jvmtiEnv* jvmti) {
  jvmtiError error;
  jvmtiPhase phase;

  error = (*jvmti)->GetPhase(jvmti, &phase);
  if (error != JVMTI_ERROR_NONE) {
    LogJvmtiError(jvmti, error, "CheckForOnLoadOrLivePhase(): GetPhase() error.");
    return JNI_ERR;
  }

  // We need to be in either the "live" or "on load" phase because all of
  // our JVMTI methods (except GenerateEvents) require one of these two phases.
  // If in the "on load" phase, we will later skip GenerateEvents.
  // https://docs.oracle.com/en/java/javase/17/docs/specs/jvmti.html#GetPhase
  // TODO(jps): is it possible to trigger Agent_OnLoad or Agent_OnAttach *not* in live or on load?
  const bool in_live_phase = phase == JVMTI_PHASE_LIVE;
  const bool in_load_phase = phase == JVMTI_PHASE_ONLOAD;
  const bool phase_ok = in_live_phase || in_load_phase;

  if (!phase_ok) {
    // Not in live or on load phase. Bail now. Returning JNI_ERR causes the startup sequence
    // to fall through, i.e. skips the remaining steps in the agent startup sequence.
    LogF("CheckForOnLoadOrLivePhase(): Not in live or on load phase.");
    return JNI_ERR;
  }

  // We are either in "live" or "on load" phase. Keep calm and carry on.
  return JNI_OK;
}

FILE* FOpenLogFile(const char* file_path) {
  FILE* f = fopen(file_path, "w");
  if (f == NULL) {
    LogF("[error] FOpenLogFile() Unable to open: %s.", file_path);
  }
  return f;
}

jint OpenLogFiles(const char* options) {
  if (options == NULL) {
    return JNI_ERR;
  }

  g_log_file_ptr = NULL;
  g_bin_file_ptr = NULL;

  char artifacts_path[512];
  if (kUsingTxtLogFile) {
    snprintf(artifacts_path, sizeof(artifacts_path), "%s%s", options,
             "-" PX_JVMTI_AGENT_HASH "/" TXT_SYMBOL_FILENAME);
    // TODO(jps): remove the txt based log file once we finalize java symbolization.
    g_log_file_ptr = FOpenLogFile(artifacts_path);
    if (g_log_file_ptr == NULL) {
      return JNI_ERR;
    }
  }
  if (kUsingBinLogFile) {
    snprintf(artifacts_path, sizeof(artifacts_path), "%s%s", options,
             "-" PX_JVMTI_AGENT_HASH "/" BIN_SYMBOL_FILENAME);
    g_bin_file_ptr = FOpenLogFile(artifacts_path);
    if (g_bin_file_ptr == NULL) {
      return JNI_ERR;
    }
  }
  LogF("OpenLogFiles(), log file path pfx: %s.", options);
  return JNI_OK;
}

jint GetJVMTIEnv(JavaVM* jvm, jvmtiEnv** jvmti) {
  if (jvm == NULL || jvmti == NULL) {
    return JNI_ERR;
  }

  jint error = (*jvm)->GetEnv(jvm, (void**)jvmti, JVMTI_VERSION_1_0);
  if (error != JNI_OK || *jvmti == NULL) {
    LogF("[error] Unable to access JVMTI.");
    return JNI_ERR;
  }
  return JNI_OK;
}

JNIEXPORT uint64_t PixieJavaAgentTestFn() {
  // This function is exported solely for the purpose of verifying that
  // dlopen + dlsym works as expected.
  // TODO(jps): move the value "42" into kExpectedJavaAgentTestFnReturnVal in a shared header.
  return 42;
}

JNIEXPORT jint JNICALL Agent_OnAttach(JavaVM* jvm, char* options, void* /*reserved*/) {
  if (g_callbacks_attached) {
    LogF("Agent_OnAttach(). g_callbacks_attached=true, skipping agent startup sequence.");
    return JNI_OK;
  }

  static jvmtiEnv* jvmti = NULL;

  PX_JVMTI_AGENT_RETURN_IF_ERROR(OpenLogFiles(options));
  PX_JVMTI_AGENT_RETURN_IF_ERROR(GetJVMTIEnv(jvm, &jvmti));
  PX_JVMTI_AGENT_RETURN_IF_ERROR(AddJVMTICapabilities(jvmti));
  PX_JVMTI_AGENT_RETURN_IF_ERROR(SetNotificationModes(jvmti));
  PX_JVMTI_AGENT_RETURN_IF_ERROR(SetCallbackFunctions(jvmti));
  PX_JVMTI_AGENT_RETURN_IF_ERROR(ReplayCallbacks(jvmti));

  LogF("Pixie JVMTI symbolization agent startup sequence complete.");
  return JNI_OK;
}

JNIEXPORT jint JNICALL Agent_OnLoad(JavaVM* jvm, char* options, void* /*reserved*/) {
  return Agent_OnAttach(jvm, options, NULL);
}
