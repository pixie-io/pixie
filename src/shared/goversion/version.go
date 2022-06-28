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
	builtBy        = "Unknown"
)

var versionInstance *Version

// Version contains the build revision/time/status information.
type Version struct {
	buildSCMRevision string
	buildSCMStatus   string
	buildSemver      semver.Version
	buildTimeStamp   time.Time
	builtBy          string
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
		builtBy,
	}
	v.Build = buildMetadata

	versionInstance = &Version{
		buildSCMRevision: buildSCMRevision,
		buildSCMStatus:   buildSCMStatus,
		buildSemver:      v,
		buildTimeStamp:   t,
		builtBy:          builtBy,
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

// Builder returns the built by.
func (v *Version) Builder() string {
	return v.builtBy
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
