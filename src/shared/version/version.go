package version

import (
	"fmt"
	"strconv"
	"time"
)

// Variables loaded from x_defs.
var buildSCMRevision string
var buildSCMStatus string
var buildTimeStamp string

var versionInstance *Version

// Version contains the build revision/time/status information.
type Version struct {
	buildSCMRevision string
	buildSCMStatus   string
	buildTimeStamp   time.Time
}

func init() {
	tUnix, err := strconv.ParseInt(buildTimeStamp, 10, 64)
	if err != nil {
		tUnix = 0
	}
	t := time.Unix(tUnix, 0)

	versionInstance = &Version{
		buildSCMRevision: buildSCMRevision,
		buildSCMStatus:   buildSCMStatus,
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

// ToString returns the git version string.
func (v *Version) ToString() string {
	return fmt.Sprintf("GIT:%s-%s, BuildTime: %s", v.Revision(), v.RevisionStatus(), v.BuildTimestamp())
}

// GetVersion returns the current version instance.
func GetVersion() *Version {
	return versionInstance
}
