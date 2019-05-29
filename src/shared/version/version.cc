#include <string>

#include "absl/strings/substitute.h"
#include "absl/time/time.h"
#include "src/shared/version/version.h"

extern const char* kBuildSCMStatus;
extern const char* kBuildSCMRevision;
extern const int64_t kBuildTimeStamp;

namespace pl {

std::string VersionInfo::Revision() { return kBuildSCMRevision; }

std::string VersionInfo::RevisionStatus() { return kBuildSCMStatus; }

std::string VersionInfo::VersionString() {
#ifdef NDEBUG
  const char* build_type = "RELEASE";
#else
  const char* build_type = "DEBUG";
#endif
  auto t = absl::FromUnixSeconds(kBuildTimeStamp);
  auto build_time = absl::FormatTime(t, absl::UTCTimeZone());
  return absl::Substitute("GIT:$0-$1, BuildType:$2, BuildTime:$3", Revision(), RevisionStatus(),
                          build_type, build_time);
}

}  // namespace pl
