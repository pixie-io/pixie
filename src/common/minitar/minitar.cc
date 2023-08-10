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

#include "src/common/minitar/minitar.h"

#include <sys/stat.h>
#include <sys/types.h>

#include <archive_entry.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <string>

#include "src/common/base/base.h"

namespace px {
namespace tools {

#define PX_RETURN_IF_NOT_ARCHIVE_OK(r, ar)              \
  {                                                     \
    if (r != ARCHIVE_OK) {                              \
      return error::Internal(archive_error_string(ar)); \
    }                                                   \
  }

Minitar::Minitar(std::filesystem::path file) : file_(file) {
  a = archive_read_new();
  ext = archive_write_disk_new();
}

Minitar::~Minitar() {
  archive_read_free(a);
  archive_write_free(ext);
}

namespace {
Status CopyData(struct archive* ar, struct archive* aw) {
  int r;
  const void* buff;
  size_t size;
  int64_t offset;

  while (true) {
    r = archive_read_data_block(ar, &buff, &size, &offset);
    if (r == ARCHIVE_EOF) {
      break;
    }
    PX_RETURN_IF_NOT_ARCHIVE_OK(r, ar);

    r = archive_write_data_block(aw, buff, size, offset);
    PX_RETURN_IF_NOT_ARCHIVE_OK(r, ar);
  }

  return Status::OK();
}
}  // namespace

Status Minitar::Extract(std::string_view dest_dir, std::string_view prefix_to_strip, int flags) {
  int r;

  // Declare the deferred closes up-front,
  // in case there is an error.
  DEFER(archive_read_close(a));
  DEFER(archive_write_close(ext));

  archive_write_disk_set_options(ext, flags);
  archive_read_support_filter_gzip(a);
  archive_read_support_format_tar(a);

  r = archive_read_open_filename(a, file_.string().c_str(), 10240);
  PX_RETURN_IF_NOT_ARCHIVE_OK(r, a);

  while (true) {
    struct archive_entry* entry;
    r = archive_read_next_header(a, &entry);
    if (r == ARCHIVE_EOF) {
      break;
    }
    PX_RETURN_IF_NOT_ARCHIVE_OK(r, a);

    // Get the original entry pathname.
    std::string_view pathname(archive_entry_pathname(entry));

    // Check if the pathname starts with the prefix to strip, and if so, remove it.
    if (!prefix_to_strip.empty() && absl::StartsWith(pathname, prefix_to_strip)) {
      pathname.remove_prefix(prefix_to_strip.length());
    }

    // if a destination directory is provided...
    if (!dest_dir.empty()) {
      // Concatenate the destination directory with the entry's pathname.
      std::string dest_path = absl::StrCat(dest_dir, "/", pathname);
      // Set the entry's pathname to the newly constructed path.
      archive_entry_set_pathname(entry, dest_path.c_str());
    }

    // Write the header for the current entry.
    r = archive_write_header(ext, entry);
    PX_RETURN_IF_NOT_ARCHIVE_OK(r, a);
    PX_RETURN_IF_ERROR(CopyData(a, ext));
  }

  return Status::OK();
}

}  // namespace tools
}  // namespace px
