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

#include <stdio.h>
#include <unistd.h>

#include <filesystem>
#include <fstream>

#include "src/common/fs/fs_wrapper.h"
#include "src/stirling/source_connectors/perf_profiler/java/agent/raw_symbol_update.h"
#include "src/stirling/source_connectors/perf_profiler/java/attach.h"
#include "src/stirling/upid/upid.h"

namespace px {
namespace stirling {

class JavaSymbolFileWriter {
 public:
  JavaSymbolFileWriter();
  void WriteSymbol(const uint64_t addr, const uint64_t size, char const* const symbol,
                   char const* const fn_sig, char const* const class_sig);
  void WriteUnload(const uint64_t addr);

 private:
  std::unique_ptr<std::ofstream> symbol_file_;
};

JavaSymbolFileWriter::JavaSymbolFileWriter() {
  constexpr uint64_t start_time_ns = 0;
  const uint32_t pid = getpid();
  const struct upid_t my_upid = {{pid}, start_time_ns};
  const std::filesystem::path artifacts_path = java::StirlingArtifactsPath(my_upid);
  const std::filesystem::path symbol_file_path = java::StirlingSymbolFilePath(my_upid);

  PX_EXIT_IF_ERROR(fs::CreateDirectories(artifacts_path));

  constexpr auto kIOFlags = std::ios::out | std::ios::binary;
  symbol_file_ = std::make_unique<std::ofstream>(symbol_file_path, kIOFlags);

  if (symbol_file_->fail()) {
    LOG(FATAL) << absl::Substitute("Could not create symbol file: $0.", symbol_file_path.string());
  }
  LOG(INFO) << absl::Substitute("Symbol file path: $0.", symbol_file_path.string());
}

void JavaSymbolFileWriter::WriteSymbol(const uint64_t addr, const uint64_t size,
                                       char const* const symbol, char const* const fn_sig,
                                       char const* const class_sig) {
  java::RawSymbolUpdate update;

  update.addr = addr;
  update.code_size = size;
  update.symbol_size = 1 + strlen(symbol);
  update.fn_sig_size = 1 + strlen(fn_sig);
  update.class_sig_size = 1 + strlen(class_sig);
  update.method_unload = 0;

  symbol_file_->write(reinterpret_cast<char*>(&update), sizeof(java::RawSymbolUpdate));
  symbol_file_->write(symbol, update.symbol_size);
  symbol_file_->write(fn_sig, update.fn_sig_size);
  symbol_file_->write(class_sig, update.class_sig_size);
}

void JavaSymbolFileWriter::WriteUnload(const uint64_t addr) {
  java::RawSymbolUpdate update;

  update.addr = addr;
  update.code_size = 0;
  update.symbol_size = 1;
  update.fn_sig_size = 1;
  update.class_sig_size = 1;
  update.method_unload = 1;
  constexpr char nullchar = '\0';

  symbol_file_->write(reinterpret_cast<char*>(&update), sizeof(java::RawSymbolUpdate));
  symbol_file_->write(&nullchar, 1);
  symbol_file_->write(&nullchar, 1);
  symbol_file_->write(&nullchar, 1);
}

void CreateFakeJavaSymbolFile() {
  JavaSymbolFileWriter writer;

  // Writing a symbol file with "known" address ranges and symbols for use in the symbolizer test.
  writer.WriteSymbol(100, 100, "doStuff", "(Lfoo/bar/TheStuff;Z)I", "Lqux/foo;");
  writer.WriteSymbol(200, 100, "doNothing", "(B;Z;I;J;F;D)V", "Lfoo/bar/baz/qux;");

  // Create a symbol from address 300 to 599.
  writer.WriteSymbol(300, 300, "compute", "(B)Z", "Lfoo/bar;");

  // Unload that previous symbol. Replace it with two new symbols (at addresses 300 to 399
  // and at 500 to 599) and leave a range of addresses (400 to 499) with no symbol.
  writer.WriteUnload(300);
  writer.WriteSymbol(300, 100, "compute", "(Z)B", "Lfoo/qux;");
  writer.WriteSymbol(500, 100, "compute", "(D)J", "Lqux/foo;");
}

}  // namespace stirling
}  // namespace px

int main() {
  // Write the symbol file (into the canonical artifacts path based on our own pid).
  // Then sleep while the symbolizer test does its thing. The symbolizer test will kill
  // this long before the call to sleep returns.
  px::stirling::CreateFakeJavaSymbolFile();
  sleep(3600);
  return 0;
}
