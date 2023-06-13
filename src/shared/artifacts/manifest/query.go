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

package manifest

import (
	"errors"
	"sort"

	"golang.org/x/mod/semver"

	"px.dev/pixie/src/shared/artifacts/versionspb"
)

// ErrArtifactSetNotFound is returned when an artifact is requested for an artifact set that's not in the manifest.
var ErrArtifactSetNotFound = errors.New("no artifact set with given artifact name")

// ErrArtifactNotFound is returned when the version of an artifact requested is not found within the requested artifact set.
var ErrArtifactNotFound = errors.New("no artifact with given version")

// GetArtifact returns a an artifact with the given artifact name and version string.
func (m *ArtifactManifest) GetArtifact(name string, version string) (*versionspb.Artifact, error) {
	as, ok := m.sets[name]
	if !ok {
		return nil, ErrArtifactSetNotFound
	}
	i := sort.Search(len(as.artifacts), func(i int) bool { return versionCompare(version, as.artifacts[i].VersionStr) })
	if i < len(as.artifacts) && (as.artifacts[i].VersionStr == version) {
		return as.artifacts[i], nil
	}
	return nil, ErrArtifactNotFound
}

// ArtifactFilter filters artifacts for a ListArtifacts call. Returning true means the artifact should be kept.
type ArtifactFilter func(*versionspb.Artifact) bool

// ListArtifacts returns artifacts with the given name, in version sorted order (newest versions first). It returns up to `limit` artifacts.
func (m *ArtifactManifest) ListArtifacts(name string, limit int64, filters ...ArtifactFilter) ([]*versionspb.Artifact, error) {
	as, ok := m.sets[name]
	if !ok {
		return nil, ErrArtifactSetNotFound
	}

	keep := func(a *versionspb.Artifact) bool {
		for _, f := range filters {
			if !f(a) {
				return false
			}
		}
		return true
	}
	ret := make([]*versionspb.Artifact, 0)
	for _, a := range as.artifacts {
		if !keep(a) {
			continue
		}
		ret = append(ret, a)
		if limit > 0 && int64(len(ret)) == limit {
			break
		}
	}
	return ret, nil
}

// RemovePrereleasesFilter filters out any artifacts that are for prerelease versions.
func RemovePrereleasesFilter() ArtifactFilter {
	return func(a *versionspb.Artifact) bool {
		return semver.Prerelease("v"+a.VersionStr) == ""
	}
}

// ArtifactTypeFilter filters out any artifacts that don't have the specified ArtifactType available.
func ArtifactTypeFilter(at versionspb.ArtifactType) ArtifactFilter {
	return func(a *versionspb.Artifact) bool {
		for _, am := range a.AvailableArtifactMirrors {
			if at == am.ArtifactType {
				return true
			}
		}
		for _, t := range a.AvailableArtifacts {
			if at == t {
				return true
			}
		}
		return false
	}
}
