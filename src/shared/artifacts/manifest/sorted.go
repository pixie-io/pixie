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
	"sort"

	"golang.org/x/mod/semver"

	"px.dev/pixie/src/shared/artifacts/versionspb"
)

// sortedArtifactSet keeps it artifacts in version descending order (newer versions first).
type sortedArtifactSet struct {
	name      string
	artifacts []*versionspb.Artifact
}

func (as *sortedArtifactSet) Len() int {
	return len(as.artifacts)
}

func (as *sortedArtifactSet) Less(i, j int) bool {
	return artifactCompare(as.artifacts[i], as.artifacts[j])
}

func (as *sortedArtifactSet) Swap(i, j int) {
	as.artifacts[i], as.artifacts[j] = as.artifacts[j], as.artifacts[i]
}

func (as *sortedArtifactSet) toProto() *versionspb.ArtifactSet {
	pb := &versionspb.ArtifactSet{
		Name:     as.name,
		Artifact: as.artifacts,
	}
	return pb
}

func newSortedArtifactSetFromProto(pb *versionspb.ArtifactSet) *sortedArtifactSet {
	as := &sortedArtifactSet{
		name:      pb.Name,
		artifacts: pb.Artifact,
	}
	sort.Sort(as)
	return as
}

func artifactCompare(a, b *versionspb.Artifact) bool {
	return versionCompare(a.VersionStr, b.VersionStr)
}

func versionCompare(a, b string) bool {
	// Sort the semantic versions in descending order.
	return semver.Compare("v"+a, "v"+b) >= 0
}
