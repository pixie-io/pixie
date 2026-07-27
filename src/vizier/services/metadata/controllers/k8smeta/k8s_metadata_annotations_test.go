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

 package k8smeta

 import (
	 "testing"
	 "github.com/stretchr/testify/assert"
	 "px.dev/pixie/src/shared/k8s/metadatapb"
 )
 
 func testPodWithAnnotations() *metadatapb.Pod {
	 return &metadatapb.Pod{
		 Metadata: &metadatapb.ObjectMetadata{
			 UID:       "ijkl",
			 Name:      "object_md",
			 Namespace: "ns",
			 Annotations: map[string]string{
				 "team":                         "pixie",
				 "kubectl.kubernetes.io/loaded": "big-blob",
			 },
		 },
		 Status: &metadatapb.PodStatus{},
		 Spec:   &metadatapb.PodSpec{},
	 }
 }
 
 func TestGetResourceUpdateFromPod_AnnotationAllowlist(t *testing.T) {
	 tests := []struct {
		 name      string
		 allowlist map[string]bool
		 expected  string
	 }{
		 {
			 name:      "empty allowlist captures nothing",
			 allowlist: map[string]bool{},
			 expected:  "",
		 },
		 {
			 name:      "nil allowlist captures nothing",
			 allowlist: nil,
			 expected:  "",
		 },
		 {
			 name:      "only allowed keys are captured",
			 allowlist: map[string]bool{"team": true},
			 expected:  `{"team":"pixie"}`,
		 },
		 {
			 name:      "non-matching key captures nothing",
			 allowlist: map[string]bool{"nonexistent": true},
			 expected:  "",
		 },
	 }
 
	 for _, tc := range tests {
		 t.Run(tc.name, func(t *testing.T) {
			 update := getResourceUpdateFromPod(testPodWithAnnotations(), 1, tc.allowlist)
			 assert.Equal(t, tc.expected, update.GetPodUpdate().Annotations)
		 })
	 }
 }