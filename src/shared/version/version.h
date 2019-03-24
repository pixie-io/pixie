#pragma once

#include <string>

namespace pl {

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
};

}  // namespace pl
