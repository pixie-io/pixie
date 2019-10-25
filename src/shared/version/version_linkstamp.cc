#include <cstdint>

extern const char* kBuildSCMStatus;
extern const char* kBuildSCMRevision;
extern const int64_t kBuildTimeStamp;
extern const char* kBuildSemver;
extern const char* kBuildNumber;

const char* kBuildSCMStatus = BUILD_SCM_STATUS;
const char* kBuildSCMRevision = BUILD_SCM_REVISION;
const int64_t kBuildTimeStamp = BUILD_TIMESTAMP;  // UNIX TIMESTAMP seconds.
const char* kBuildSemver = BUILD_TAG;             // Semver string.
const char* kBuildNumber = BUILD_NUMBER;          // Build number..
