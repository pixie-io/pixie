#include <string>

#include <absl/strings/numbers.h>
#include <absl/strings/substitute.h>
#include <absl/time/time.h>
#include "src/shared/version/version.h"

extern const char* kBuildSCMStatus;
extern const char* kBuildSCMRevision;
extern const int64_t kBuildTimeStamp;
extern const char* kBuildSemver;
extern const char* kBuildNumber;

namespace px {

std::string VersionInfo::Revision() { return kBuildSCMRevision; }

std::string VersionInfo::RevisionStatus() { return kBuildSCMStatus; }

int VersionInfo::BuildNumber() {
  int build_number = 0;
  bool ok = absl::SimpleAtoi(kBuildNumber, &build_number);
  return ok ? build_number : 0;
}

std::string VersionInfo::VersionString() {
#ifdef NDEBUG
  const char* build_type = "RELEASE";
#else
  const char* build_type = "DEBUG";
#endif
  std::string short_rev = Revision().substr(0, 7);
  auto t = absl::FromUnixSeconds(kBuildTimeStamp);
  auto build_time = absl::FormatTime("%Y%m%d%H%M", t, absl::LocalTimeZone());
  return absl::Substitute("v$0+$1.$2.$3.$4.$5", kBuildSemver, RevisionStatus(), short_rev,
                          build_time, BuildNumber(), build_type);
}

}  // namespace px
