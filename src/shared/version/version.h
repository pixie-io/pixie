#pragma once

#include <string>

namespace px {

class VersionInfo {
 public:
  /**
   * Return the revision ID.
   * @return string
   */
  static std::string Revision();

  /**
   * Get the repository status.
   * @return string
   */
  static std::string RevisionStatus();

  /**
   * Returns a version string with git info, release status.
   * @return string
   */
  static std::string VersionString();

  /**
   * Returns the build number if present (or 0).
   * @return int build number
   */
  static int BuildNumber();
};

}  // namespace px
