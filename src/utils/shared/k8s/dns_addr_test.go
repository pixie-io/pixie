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

package k8s_test

import (
	"testing"

	"github.com/stretchr/testify/assert"
	v1 "k8s.io/api/core/v1"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"

	"px.dev/pixie/src/utils/shared/k8s"
)

func TestGetPodAddr(t *testing.T) {
	keyValueStringToMapTests := []struct {
		pod          v1.Pod
		expectedAddr string
	}{
		{
			pod: v1.Pod{
				Status: v1.PodStatus{
					PodIP: "1.2.3.4",
				},
				ObjectMeta: metav1.ObjectMeta{
					Namespace: "test-ns",
				},
			},
			expectedAddr: "1-2-3-4.test-ns.pod.cluster.local",
		},
		{
			pod: v1.Pod{
				Status: v1.PodStatus{
					PodIP: "2001:0db8:85a3:0000:0000:8a2e:0370:7334",
				},
				ObjectMeta: metav1.ObjectMeta{
					Namespace: "test-ns",
				},
			},
			expectedAddr: "2001-0db8-85a3-0000-0000-8a2e-0370-7334.test-ns.pod.cluster.local",
		},
	}

	for _, tc := range keyValueStringToMapTests {
		assert.Equal(t, tc.expectedAddr, k8s.GetPodAddr(tc.pod))
	}
}
