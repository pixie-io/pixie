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

	"github.com/gogo/protobuf/proto"

	"px.dev/pixie/src/shared/artifacts/versionspb"
)

// Merge returns a new ArtifactManifest with the artifacts merged. On conflicts, values in `updates` take precedence.
func (a *ArtifactManifest) Merge(updates *ArtifactManifest) *ArtifactManifest {
	merged := &ArtifactManifest{
		sets: make(map[string]*sortedArtifactSet),
	}
	for name, set := range a.sets {
		merged.sets[name] = set
	}
	for name, set := range updates.sets {
		base, ok := merged.sets[name]
		if !ok {
			merged.sets[name] = set
			continue
		}
		merged.sets[name] = base.Merge(set)
	}
	return merged
}

func (as *sortedArtifactSet) Merge(updates *sortedArtifactSet) *sortedArtifactSet {
	merged := &sortedArtifactSet{
		name:      as.name,
		artifacts: as.artifacts[:],
	}
	for _, a := range updates.artifacts {
		baseIndex := -1
		for i, other := range merged.artifacts {
			if a.VersionStr == other.VersionStr {
				baseIndex = i
			}
		}
		if baseIndex == -1 {
			merged.artifacts = append(merged.artifacts, a)
			continue
		}
		merged.artifacts[baseIndex] = mergeArtifact(merged.artifacts[baseIndex], a)
	}
	sort.Sort(merged)
	return merged
}

func mergeArtifact(base, updates *versionspb.Artifact) *versionspb.Artifact {
	merged := proto.Clone(base).(*versionspb.Artifact)
	merged.Timestamp = updates.Timestamp
	merged.CommitHash = updates.CommitHash
	merged.AvailableArtifacts = updates.AvailableArtifacts
	merged.AvailableArtifactMirrors = updates.AvailableArtifactMirrors
	merged.Changelog = updates.Changelog
	return merged
}
