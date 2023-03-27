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
	"encoding/json"
	"io"

	"px.dev/pixie/src/shared/artifacts/versionspb"
)

// ArtifactManifest represents a manifest file listing artifact sets.
// Internally, the artifacts are sorted by their versions, with newer versions first.
type ArtifactManifest struct {
	sets map[string]*sortedArtifactSet
}

// ReadArtifactManifest reads an ArtifactManifest from a json stream.
func ReadArtifactManifest(r io.Reader) (*ArtifactManifest, error) {
	dec := json.NewDecoder(r)
	m := &ArtifactManifest{}
	if err := dec.Decode(m); err != nil {
		return nil, err
	}
	return m, nil
}

// Write writes an artifact manifest in JSON format to the given io.Writer.
func (a *ArtifactManifest) Write(w io.Writer) error {
	enc := json.NewEncoder(w)
	return enc.Encode(a)
}

// ArtifactSets returns the list of protobuf artifact sets in this manifest.
func (a *ArtifactManifest) ArtifactSets() []*versionspb.ArtifactSet {
	pb := make([]*versionspb.ArtifactSet, 0, len(a.sets))
	for _, as := range a.sets {
		pb = append(pb, as.toProto())
	}
	return pb
}

// NewArtifactManifestFromProto returns a new ArtifactManifest from a list of ArtifactSet protobufs.
func NewArtifactManifestFromProto(pb []*versionspb.ArtifactSet) *ArtifactManifest {
	m := &ArtifactManifest{
		sets: make(map[string]*sortedArtifactSet, len(pb)),
	}
	for _, as := range pb {
		m.sets[as.Name] = newSortedArtifactSetFromProto(as)
	}
	return m
}
