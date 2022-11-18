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
	"encoding/json"
	"errors"
	"fmt"
	"path"
	"strconv"
	"strings"

	"github.com/gogo/protobuf/proto"

	"px.dev/pixie/src/vizier/services/metadata/storepb"
	"px.dev/pixie/src/vizier/utils/datastore"
)

const (
	fullResourceUpdatePrefix  = "/fullResourceUpdate"
	topicResourceUpdatePrefix = "/resourceUpdate"
	topicVersionPrefix        = "/topicVersion"
	labelPodUpdatePrefix      = "/labelPodUpdate" // labelPodUpdatePrefix/<namespace>/<labelKey>/<podName> -> <labelValue>
	podLabelUpdatePrefix      = "/podLabelUpdate" // podLabelUpdatePrefix/<namespace>/<podName> -> [<labelKeys>]
	// The topic for partial resource updates, which are not specific to a particular node.
	unscopedTopic = "unscoped"
)

// Datastore implements the Store interface on a given Datastore.
type Datastore struct {
	ds datastore.MultiGetterSetterDeleterCloser
}

// NewDatastore wraps the datastore in a metadata store.
func NewDatastore(ds datastore.MultiGetterSetterDeleterCloser) *Datastore {
	return &Datastore{ds}
}

func getFullResourceUpdateKey(version int64) string {
	return path.Join(fullResourceUpdatePrefix, fmt.Sprintf("%020d", version))
}

func getTopicResourceUpdateKey(topic string, version int64) string {
	return path.Join(topicResourceUpdatePrefix, topic, fmt.Sprintf("%020d", version))
}

func getTopicVersionKey(topic string) string {
	return path.Join(topicVersionPrefix, topic)
}

// prefix/<namespace>/<labelKey>/<podName>
func getNSLabelPodUpdateKey(namespace string, labelKey string, podName string) string {
	return path.Join(labelPodUpdatePrefix, namespace, labelKey, podName)
}

// prefix/<namespace>/<labelKey>
func getNSLabelPrefixKey(namespace string, labelKey string) string {
	return path.Join(labelPodUpdatePrefix, namespace, labelKey)
}

// prefix/<namespace>/<podName>
func getNSPodUpdateKey(namespace string, podName string) string {
	return path.Join(podLabelUpdatePrefix, namespace, podName)
}

func labelPodUpdateKeyToPodName(updateKey string) string {
	keys := strings.Split(updateKey, "/")
	return keys[len(keys)-1]
}

func resourceUpdateKeyToUpdateVersion(key string) (int64, error) {
	splitKey := strings.Split(key, "/")
	if len(splitKey) != 4 {
		return 0, errors.New("Invalid key")
	}
	return strconv.ParseInt(strings.TrimLeft(splitKey[3], "0"), 10, 64)
}

// AddResourceUpdateForTopic stores the given resource with its associated updateVersion for 24h.
func (m *Datastore) AddResourceUpdateForTopic(updateVersion int64, topic string, resource *storepb.K8SResourceUpdate) error {
	val, err := resource.Marshal()
	if err != nil {
		return err
	}

	return m.ds.SetWithTTL(getTopicResourceUpdateKey(topic, updateVersion), string(val), resourceUpdateTTL)
}

// AddResourceUpdate stores a resource update that is applicable to all topics.
func (m *Datastore) AddResourceUpdate(updateVersion int64, resource *storepb.K8SResourceUpdate) error {
	return m.AddResourceUpdateForTopic(updateVersion, unscopedTopic, resource)
}

// AddFullResourceUpdate stores full resource update with the given update version.
func (m *Datastore) AddFullResourceUpdate(updateVersion int64, resource *storepb.K8SResource) error {
	val, err := resource.Marshal()
	if err != nil {
		return err
	}

	return m.ds.SetWithTTL(getFullResourceUpdateKey(updateVersion), string(val), resourceUpdateTTL)
}

// FetchResourceUpdates gets the resource updates from the `from` update version, to the `to`
// update version (exclusive).
func (m *Datastore) FetchResourceUpdates(topic string, from int64, to int64) ([]*storepb.K8SResourceUpdate, error) {
	// Fetch updates specific to the topic.
	tKeys, tValues, err := m.ds.GetWithRange(getTopicResourceUpdateKey(topic, from), getTopicResourceUpdateKey(topic, to))
	if err != nil {
		return nil, err
	}

	// Fetch updates that are unscoped and shared across all nodes.
	uKeys, uValues, err := m.ds.GetWithRange(getTopicResourceUpdateKey(unscopedTopic, from), getTopicResourceUpdateKey(unscopedTopic, to))
	if err != nil {
		return nil, err
	}

	updates := make([]*storepb.K8SResourceUpdate, 0)

	// Merge the topic-specific and unscoped updates in order.
	tIdx := 0
	uIdx := 0
	for {
		if tIdx == len(tKeys) && uIdx == len(uKeys) {
			break
		}

		var uVersion int64 = -1
		var tVersion int64 = -1
		var err error

		if tIdx < len(tKeys) {
			tVersion, err = resourceUpdateKeyToUpdateVersion(tKeys[tIdx])
			if err != nil { // Malformed key, skip it.
				tIdx++
				continue
			}
		}

		if uIdx < len(uKeys) {
			uVersion, err = resourceUpdateKeyToUpdateVersion(uKeys[uIdx])
			if err != nil { // Malformed key, skip it.
				uIdx++
				continue
			}
		}

		var updateBytes []byte
		switch {
		case tVersion == -1 || (uVersion != -1 && uVersion < tVersion):
			updateBytes = uValues[uIdx]
			uIdx++
		case uVersion == -1 || (tVersion < uVersion):
			updateBytes = tValues[tIdx]
			tIdx++
		default:
			// tVersion == uVersion. This should never happen, since a resource update must either be node-specific or not. In any
			// case, if this ever happens, we should just take the topic-specific update.
			updateBytes = tValues[tIdx]
			tIdx++
			uIdx++
		}

		updatePb := &storepb.K8SResourceUpdate{}
		err = proto.Unmarshal(updateBytes, updatePb)
		if err != nil {
			continue
		}
		updates = append(updates, updatePb)
	}

	return updates, nil
}

// FetchFullResourceUpdates gets the full resource updates from the `from` update version, to the `to`
// update version (exclusive).
func (m *Datastore) FetchFullResourceUpdates(from int64, to int64) ([]*storepb.K8SResource, error) {
	// Fetch updates specific to the topic.
	_, vals, err := m.ds.GetWithRange(getFullResourceUpdateKey(from), getFullResourceUpdateKey(to))
	if err != nil {
		return nil, err
	}

	updates := make([]*storepb.K8SResource, 0)
	for _, u := range vals {
		updatePb := &storepb.K8SResource{}
		err = proto.Unmarshal(u, updatePb)
		if err != nil {
			continue
		}
		updates = append(updates, updatePb)
	}

	return updates, nil
}

// GetUpdateVersion gets the last update version sent on a topic.
func (m *Datastore) GetUpdateVersion(topic string) (int64, error) {
	val, err := m.ds.Get(getTopicVersionKey(topic))
	if err != nil {
		return 0, err
	}

	if string(val) == "" {
		return 0, nil
	}

	return strconv.ParseInt(string(val), 10, 64)
}

// SetUpdateVersion sets the last update version sent on a topic.
func (m *Datastore) SetUpdateVersion(topic string, uv int64) error {
	s := strconv.FormatInt(uv, 10)

	return m.ds.Set(getTopicVersionKey(topic), s)
}

// SetPodLabels stores the pod labels information. `<namespace>/<labelKey>/<podName>` is the key and
// `<labelValue>` is the value. The current implementation assumes that the pod resource update contains
// the ground truth for all the labels on this pod (verified by playing around with the k8s api). If an
// old label no longer shows up in the current update, it will be deleted from the PodLabelStore.
func (m *Datastore) SetPodLabels(namespace string, podName string, labels map[string]string) error {
	nsPodKey := getNSPodUpdateKey(namespace, podName)

	// Get existing labelKeys of this pod.
	existingKeys := make(map[string]bool)
	b, err := m.ds.Get(nsPodKey)
	if err != nil {
		return err
	}
	if b != nil {
		err = json.Unmarshal(b, &existingKeys)
		if err != nil {
			return err
		}
	}

	// For each new labelKey, update the nsLabelPod and record the new keys.
	keys := make(map[string]bool, len(labels))
	for k, v := range labels {
		err := m.ds.SetWithTTL(getNSLabelPodUpdateKey(namespace, k, podName), v, resourceUpdateTTL)
		if err != nil {
			return err
		}
		keys[k] = true
	}

	// For each existingKey not in the new labelKeys, record nsLabelPod update key and delete all.
	keysToDelete := make([]string, 0)
	for k := range existingKeys {
		if keys[k] {
			continue
		}
		keysToDelete = append(keysToDelete, getNSLabelPodUpdateKey(namespace, k, podName))
	}
	if len(keysToDelete) > 0 {
		err = m.ds.DeleteAll(keysToDelete)
		if err != nil {
			return err
		}
	}

	// Set the nsPodKey with new labelKeys.
	b, err = json.Marshal(keys)
	if err != nil {
		return err
	}
	err = m.ds.SetWithTTL(nsPodKey, string(b), resourceUpdateTTL)
	if err != nil {
		return err
	}
	return nil
}

// DeletePodLabels deletes the labels information associated with a pod.
func (m *Datastore) DeletePodLabels(namespace string, podName string) error {
	nsPodKey := getNSPodUpdateKey(namespace, podName)
	b, err := m.ds.Get(nsPodKey)
	if err != nil {
		return err
	}
	// If nsPodKey doesn't exist, we are done.
	if b == nil {
		return nil
	}

	labelKeys := make(map[string]bool)
	err = json.Unmarshal([]byte(b), &labelKeys)
	if err != nil {
		return err
	}

	// Delete all keys matching podName in the namespace.
	var keysToDelete []string
	for labelKey := range labelKeys {
		keysToDelete = append(keysToDelete, getNSLabelPodUpdateKey(namespace, labelKey, podName))
	}
	keysToDelete = append(keysToDelete, nsPodKey)
	return m.ds.DeleteAll(keysToDelete)
}

// FetchPodsWithLabelKey gets the names of all the pods that has a certain label key.
func (m *Datastore) FetchPodsWithLabelKey(namespace string, key string) ([]string, error) {
	ks, _, err := m.ds.GetWithPrefix(getNSLabelPrefixKey(namespace, key))
	if err != nil {
		return nil, err
	}

	pods := make([]string, 0, len(ks))
	for _, k := range ks {
		pods = append(pods, labelPodUpdateKeyToPodName(k))
	}
	return pods, nil
}

// FetchPodsWithLabels gets the names of all the pods whose labels match exactly all the labels provided.
func (m *Datastore) FetchPodsWithLabels(namespace string, labels map[string]string) ([]string, error) {
	// TODO(chengruizhe): Support resolving to pods in all namespaces if namespace is empty.

	// pods records the number of label matches for all the pods with keys in labels.
	pods := make(map[string]int)
	var result []string

	for lk, lv := range labels {
		updateKeys, labelValues, err := m.ds.GetWithPrefix(getNSLabelPrefixKey(namespace, lk))
		if err != nil {
			return nil, err
		}
		for i, v := range labelValues {
			if lv != string(v) {
				continue
			}
			podName := labelPodUpdateKeyToPodName(updateKeys[i])
			pods[podName]++
		}
	}

	// Filter for pods that matched all the labels.
	for p := range pods {
		if pods[p] == len(labels) {
			result = append(result, p)
		}
	}

	return result, nil
}

// GetWithPrefix gets all keys and values with the given prefix, for debugging purposes.
func (m *Datastore) GetWithPrefix(prefix string) ([]string, [][]byte, error) {
	return m.ds.GetWithPrefix(prefix)
}
