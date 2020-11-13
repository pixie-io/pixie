package version

import (
	"strconv"
	"time"

	"github.com/blang/semver"
)

// Variables loaded from x_defs. Default values specified.
var (
	buildSCMRevision = "0000000"
	buildSCMStatus   = "Modified"
	// Tracks the semver string X.Y.Z-(pre)+build
	buildSemver    = "0.0.0-dev"
	buildTimeStamp = "0"
	buildNumber    = "0"
)

var versionInstance *Version

// Version contains the build revision/time/status information.
type Version struct {
	buildSCMRevision string
	buildSCMStatus   string
	buildSemver      semver.Version
	buildTimeStamp   time.Time
	isDev            bool
}

func init() {
	tUnix, err := strconv.ParseInt(buildTimeStamp, 10, 64)
	if err != nil {
		tUnix = 0
	}

	t := time.Unix(tUnix, 0)
	v := semver.MustParse(buildSemver)

	// Short git tags are only 7 characters.
	buildSCMRevisionShort := "0000000"
	if len(buildSCMRevision) >= 7 {
		buildSCMRevisionShort = buildSCMRevision[:7]
	}

	// Add the build metadata to our version string.
	buildMetadata := []string{
		buildSCMStatus,
		buildSCMRevisionShort,
		t.Format("20060102150405"),
		buildNumber,
	}
	v.Build = buildMetadata

	versionInstance = &Version{
		buildSCMRevision: buildSCMRevision,
		buildSCMStatus:   buildSCMStatus,
		buildSemver:      v,
		buildTimeStamp:   t,
	}
}

// Revision returns the revision string.
func (v *Version) Revision() string {
	return v.buildSCMRevision
}

// RevisionStatus returns the revision status.
func (v *Version) RevisionStatus() string {
	return v.buildSCMStatus
}

// BuildTimestamp returns the build timestamp as a UTC string.
func (v *Version) BuildTimestamp() string {
	return v.buildTimeStamp.UTC().String()
}

// ToString returns the semver string.
func (v *Version) ToString() string {
	return v.buildSemver.String()
}

// Semver returns the semantic version.
func (v *Version) Semver() semver.Version {
	return v.buildSemver
}

// IsDev returns true if dev build.
func (v *Version) IsDev() bool {
	s := v.buildSemver
	return s.Major == 0 && s.Minor == 0 && s.Patch == 0
}

// GetVersion returns the current version instance.
func GetVersion() *Version {
	return versionInstance
}
