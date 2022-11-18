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

package testutils

import (
	"path"
	"strings"
)

// InMemoryPodLabelStore implements the PodLabelStore interface for testing.
type InMemoryPodLabelStore struct {
	Store map[string]string
}

const (
	labelPodUpdatePrefix = "/labelPodUpdate"
	namespaceIdx         = 2
	labelKeyIdx          = 3
	podNameIdx           = 4
)

// SetPodLabels stores the pod labels information.
func (s *InMemoryPodLabelStore) SetPodLabels(namespace string, podName string, labels map[string]string) error {
	for k, v := range labels {
		updateKey := path.Join(labelPodUpdatePrefix, namespace, k, podName)
		s.Store[updateKey] = v
	}
	return nil
}

// DeletePodLabels deletes the labels information associated with a pod.
func (s *InMemoryPodLabelStore) DeletePodLabels(namespace string, podName string) error {
	keysToDelete := make([]string, 0)
	for k := range s.Store {
		elements := strings.Split(k, "/")
		if namespace == elements[namespaceIdx] && podName == elements[podNameIdx] {
			keysToDelete = append(keysToDelete, k)
		}
	}

	for _, k := range keysToDelete {
		delete(s.Store, k)
	}
	return nil
}

// FetchPodsWithLabelKey gets the names of all the pods that has a certain label key.
func (s *InMemoryPodLabelStore) FetchPodsWithLabelKey(namespace string, key string) ([]string, error) {
	result := make([]string, 0)
	for k := range s.Store {
		elements := strings.Split(k, "/")
		if namespace == elements[namespaceIdx] && key == elements[labelKeyIdx] {
			result = append(result, elements[podNameIdx])
		}
	}
	return result, nil
}

// FetchPodsWithLabels gets the names of all the pods whose labels match exactly all the labels provided.
func (s *InMemoryPodLabelStore) FetchPodsWithLabels(namespace string, labels map[string]string) ([]string, error) {
	m := make(map[string]int)
	for lk, lv := range labels {
		for k, v := range s.Store {
			elements := strings.Split(k, "/")
			if namespace == elements[namespaceIdx] && lk == elements[labelKeyIdx] && lv == v {
				m[elements[podNameIdx]]++
			}
		}
	}

	result := make([]string, 0)
	for podName, count := range m {
		if count == len(labels) {
			result = append(result, podName)
		}
	}
	return result, nil
}

// GetWithPrefix gets all keys and values with the given prefix, for debugging purposes.
func (s *InMemoryPodLabelStore) GetWithPrefix(prefix string) ([]string, [][]byte, error) {
	return nil, nil, nil
}
