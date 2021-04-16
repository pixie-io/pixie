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

#define PL_RETURN_IF_NOT_ARCHIVE_OK(r, ar)              \
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
    PL_RETURN_IF_NOT_ARCHIVE_OK(r, ar);

    r = archive_write_data_block(aw, buff, size, offset);
    PL_RETURN_IF_NOT_ARCHIVE_OK(r, ar);
  }

  return Status::OK();
}
}  // namespace

Status Minitar::Extract(std::string_view dest_dir, int flags) {
  int r;

  // Declare the deferred closes up-front,
  // in case there is an error.
  DEFER(archive_read_close(a));
  DEFER(archive_write_close(ext));

  archive_write_disk_set_options(ext, flags);
  archive_read_support_filter_gzip(a);
  archive_read_support_format_tar(a);

  r = archive_read_open_filename(a, file_.string().c_str(), 10240);
  PL_RETURN_IF_NOT_ARCHIVE_OK(r, a);

  while (true) {
    struct archive_entry* entry;
    r = archive_read_next_header(a, &entry);
    if (r == ARCHIVE_EOF) {
      break;
    }
    PL_RETURN_IF_NOT_ARCHIVE_OK(r, a);

    if (!dest_dir.empty()) {
      std::string dest_path = absl::StrCat(dest_dir, "/", archive_entry_pathname(entry));
      archive_entry_set_pathname(entry, dest_path.c_str());
    }

    r = archive_write_header(ext, entry);
    PL_RETURN_IF_NOT_ARCHIVE_OK(r, a);
    PL_RETURN_IF_ERROR(CopyData(a, ext));
  }

  return Status::OK();
}

}  // namespace tools
}  // namespace px
