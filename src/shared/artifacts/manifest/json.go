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
	"bytes"
	"encoding/json"
	"sort"

	"github.com/gogo/protobuf/jsonpb"

	"px.dev/pixie/src/shared/artifacts/versionspb"
)

// UnmarshalJSON decodes a json array of artifact sets into an ArtifactManifest.
func (a *ArtifactManifest) UnmarshalJSON(b []byte) error {
	a.sets = make(map[string]*sortedArtifactSet)
	l := []*sortedArtifactSet{}
	if err := json.Unmarshal(b, &l); err != nil {
		return err
	}
	for _, as := range l {
		a.sets[as.name] = as
	}
	return nil
}

// MarshalJSON marshals a slice of artifact sets into a json array.
func (a *ArtifactManifest) MarshalJSON() ([]byte, error) {
	l := make([]*sortedArtifactSet, 0, len(a.sets))
	for _, as := range a.sets {
		l = append(l, as)
	}
	// marshal ArtifactSets sorted by Name.
	sort.Slice(l, func(i, j int) bool {
		return l[i].name < l[j].name
	})
	return json.Marshal(l)
}

func (as *sortedArtifactSet) UnmarshalJSON(b []byte) error {
	pb := &versionspb.ArtifactSet{}
	u := &jsonpb.Unmarshaler{
		AllowUnknownFields: true,
	}
	if err := u.Unmarshal(bytes.NewReader(b), pb); err != nil {
		return err
	}
	as.name = pb.Name
	as.artifacts = pb.Artifact
	sort.Sort(as)
	return nil
}

func (as *sortedArtifactSet) MarshalJSON() ([]byte, error) {
	pb := as.toProto()
	var b bytes.Buffer
	if err := (&jsonpb.Marshaler{}).Marshal(&b, pb); err != nil {
		return nil, err
	}
	return b.Bytes(), nil
}
