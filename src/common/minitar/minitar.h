#pragma once

#include <archive.h>

#include <filesystem>
#include <string>

#include "src/common/base/base.h"

namespace px {
namespace tools {

class Minitar {
 public:
  /**
   * Creates a new tar extractor for the given file.
   * @param tarball to extract.
   */
  explicit Minitar(std::filesystem::path file);

  ~Minitar();

  /**
   * Extract the files from the tarball initialized by the constructor.
   * @param dest_dir Directory in which to extract the tarball.
   * @param flags Controls the attributes of the files. Some possible values include:
   * ARCHIVE_EXTRACT_TIME, ARCHIVE_EXTRACT_PERM, ARCHIVE_EXTRACT_ACL, ARCHIVE_EXTRACT_FFLAGS.
   * Multiple flags can be set through the or oeprator.
   * See libarchive for flag defintitions and other possible flags.
   * @return error if the tarball could not be extracted.
   */
  Status Extract(std::string_view dest_dir = {}, int flags = kDefaultFlags);

 private:
  static constexpr int kDefaultFlags = ARCHIVE_EXTRACT_TIME;
  struct archive* a;
  struct archive* ext;

  std::filesystem::path file_;
};

}  // namespace tools
}  // namespace px
