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
	"fmt"
	"os"
	"path"
	"reflect"
	"strconv"
	"testing"
	"time"

	"github.com/cockroachdb/pebble"
	"github.com/cockroachdb/pebble/vfs"
	"github.com/gogo/protobuf/proto"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"px.dev/pixie/src/shared/k8s/metadatapb"
	"px.dev/pixie/src/vizier/services/metadata/storepb"
	"px.dev/pixie/src/vizier/utils/datastore/pebbledb"
)

func setupMDSTest(t *testing.T) (*pebbledb.DataStore, *Datastore, func()) {
	memFS := vfs.NewMem()
	c, err := pebble.Open("test", &pebble.Options{
		FS: memFS,
	})
	if err != nil {
		t.Fatal("failed to initialize a pebbledb")
		os.Exit(1)
	}

	db := pebbledb.New(c, 3*time.Second)
	ts := NewDatastore(db)
	cleanup := func() {
		err := db.Close()
		if err != nil {
			t.Fatal("failed to close db")
		}
	}

	return db, ts, cleanup
}

func TestDatastore_AddFullResourceUpdate(t *testing.T) {
	db, mds, cleanup := setupMDSTest(t)
	defer cleanup()

	update := &storepb.K8SResource{
		Resource: &storepb.K8SResource_Namespace{
			Namespace: &metadatapb.Namespace{
				Metadata: &metadatapb.ObjectMetadata{
					Name:            "object_md",
					UID:             "ijkl",
					ResourceVersion: "1",
					OwnerReferences: []*metadatapb.OwnerReference{
						{
							Kind: "pod",
							Name: "test",
							UID:  "abcd",
						},
					},
					CreationTimestampNS: 4,
					DeletionTimestampNS: 6,
				},
			},
		},
	}

	err := mds.AddFullResourceUpdate(int64(15), update)
	require.NoError(t, err)

	savedResourceUpdate, err := db.Get(path.Join(fullResourceUpdatePrefix, "00000000000000000015"))
	require.NoError(t, err)
	savedResourceUpdatePb := &storepb.K8SResource{}
	err = proto.Unmarshal(savedResourceUpdate, savedResourceUpdatePb)
	require.NoError(t, err)
	assert.Equal(t, update, savedResourceUpdatePb)
}

func TestDatastore_FetchFullResourceUpdates(t *testing.T) {
	db, mds, cleanup := setupMDSTest(t)
	defer cleanup()

	update1 := &storepb.K8SResource{
		Resource: &storepb.K8SResource_Namespace{
			Namespace: &metadatapb.Namespace{
				Metadata: &metadatapb.ObjectMetadata{
					Name:            "object_md",
					UID:             "ijkl",
					ResourceVersion: "1",
					OwnerReferences: []*metadatapb.OwnerReference{
						{
							Kind: "pod",
							Name: "test",
							UID:  "abcd",
						},
					},
					CreationTimestampNS: 4,
					DeletionTimestampNS: 6,
				},
			},
		},
	}
	val, err := update1.Marshal()
	require.NoError(t, err)
	err = db.Set(path.Join(fullResourceUpdatePrefix, fmt.Sprintf("%020d", 1)), string(val))
	require.NoError(t, err)

	update2 := &storepb.K8SResource{
		Resource: &storepb.K8SResource_Namespace{
			Namespace: &metadatapb.Namespace{
				Metadata: &metadatapb.ObjectMetadata{
					Name:            "object_md",
					UID:             "abcd",
					ResourceVersion: "2",
					OwnerReferences: []*metadatapb.OwnerReference{
						{
							Kind: "pod",
							Name: "test",
							UID:  "abcd",
						},
					},
					CreationTimestampNS: 4,
					DeletionTimestampNS: 6,
				},
			},
		},
	}
	val, err = update2.Marshal()
	require.NoError(t, err)

	err = db.Set(path.Join(fullResourceUpdatePrefix, fmt.Sprintf("%020d", 2)), string(val))
	require.NoError(t, err)

	updates, err := mds.FetchFullResourceUpdates(int64(1), int64(3))
	require.NoError(t, err)
	assert.Equal(t, 2, len(updates))
	assert.Equal(t, update1, updates[0])
	assert.Equal(t, update2, updates[1])
}

func TestDatastore_AddResourceUpdateForTopic(t *testing.T) {
	db, mds, cleanup := setupMDSTest(t)
	defer cleanup()

	update := &storepb.K8SResourceUpdate{
		Update: &metadatapb.ResourceUpdate{
			UpdateVersion: 2,
			Update: &metadatapb.ResourceUpdate_NamespaceUpdate{
				NamespaceUpdate: &metadatapb.NamespaceUpdate{
					UID:              "ijkl",
					Name:             "object_md",
					StartTimestampNS: 4,
					StopTimestampNS:  6,
				},
			},
		},
	}

	err := mds.AddResourceUpdateForTopic(int64(15), "127.0.0.1", update)
	require.NoError(t, err)

	savedResourceUpdate, err := db.Get(path.Join(topicResourceUpdatePrefix, "127.0.0.1", "00000000000000000015"))
	require.NoError(t, err)
	savedResourceUpdatePb := &storepb.K8SResourceUpdate{}
	err = proto.Unmarshal(savedResourceUpdate, savedResourceUpdatePb)
	require.NoError(t, err)
	assert.Equal(t, update, savedResourceUpdatePb)
}

func TestDatastore_AddResourceUpdate(t *testing.T) {
	db, mds, cleanup := setupMDSTest(t)
	defer cleanup()

	update := &storepb.K8SResourceUpdate{
		Update: &metadatapb.ResourceUpdate{
			UpdateVersion: 2,
			Update: &metadatapb.ResourceUpdate_NamespaceUpdate{
				NamespaceUpdate: &metadatapb.NamespaceUpdate{
					UID:              "ijkl",
					Name:             "object_md",
					StartTimestampNS: 4,
					StopTimestampNS:  6,
				},
			},
		},
	}

	err := mds.AddResourceUpdate(int64(15), update)
	require.NoError(t, err)

	savedResourceUpdate, err := db.Get(path.Join(topicResourceUpdatePrefix, unscopedTopic, "00000000000000000015"))
	require.NoError(t, err)
	savedResourceUpdatePb := &storepb.K8SResourceUpdate{}
	err = proto.Unmarshal(savedResourceUpdate, savedResourceUpdatePb)
	require.NoError(t, err)
	assert.Equal(t, update, savedResourceUpdatePb)
}

func TestDatastore_FetchResourceUpdates(t *testing.T) {
	tests := []struct {
		name                  string
		from                  int
		to                    int
		topicSpecificVersions []int
		unscopedVersions      []int
		fetchedVersions       []int
	}{
		{
			name:                  "mix1",
			topicSpecificVersions: []int{5, 6, 8, 20, 53, 56},
			unscopedVersions:      []int{2, 4, 7, 12, 13, 40},
			fetchedVersions:       []int{2, 4, 5, 6, 7, 8, 12, 13, 20, 40},
			from:                  0,
			to:                    53,
		},
		{
			name:                  "mix2",
			topicSpecificVersions: []int{5, 6, 8, 20, 53, 56},
			unscopedVersions:      []int{2, 4, 7, 12, 13, 40},
			fetchedVersions:       []int{20, 40, 53, 56},
			from:                  14,
			to:                    57,
		},
		{
			name:                  "equal",
			topicSpecificVersions: []int{4, 5, 7, 8, 10},
			unscopedVersions:      []int{6, 8, 11},
			fetchedVersions:       []int{5, 6, 7, 8, 10, 11},
			from:                  5,
			to:                    12,
		},
		{
			name:                  "topic empty",
			topicSpecificVersions: []int{},
			unscopedVersions:      []int{2, 4, 6, 8, 10},
			fetchedVersions:       []int{4, 6, 8},
			from:                  4,
			to:                    10,
		},
		{
			name:                  "unscoped empty",
			topicSpecificVersions: []int{2, 4, 6, 8, 10},
			unscopedVersions:      []int{},
			fetchedVersions:       []int{4, 6, 8},
			from:                  4,
			to:                    10,
		},
	}

	for _, tc := range tests {
		t.Run(tc.name, func(t *testing.T) {
			db, mds, cleanup := setupMDSTest(t)
			defer cleanup()

			// Insert updates into db.
			for _, u := range tc.topicSpecificVersions {
				update := &storepb.K8SResourceUpdate{
					Update: &metadatapb.ResourceUpdate{
						UpdateVersion: int64(u),
					},
				}
				val, err := update.Marshal()
				require.NoError(t, err)

				err = db.Set(path.Join(topicResourceUpdatePrefix, "127.0.0.1", fmt.Sprintf("%020d", u)), string(val))
				require.NoError(t, err)
			}
			for _, u := range tc.unscopedVersions {
				update := &storepb.K8SResourceUpdate{
					Update: &metadatapb.ResourceUpdate{
						UpdateVersion: int64(u),
					},
				}
				val, err := update.Marshal()
				require.NoError(t, err)

				err = db.Set(path.Join(topicResourceUpdatePrefix, unscopedTopic, fmt.Sprintf("%020d", u)), string(val))
				require.NoError(t, err)
			}

			updates, err := mds.FetchResourceUpdates("127.0.0.1", int64(tc.from), int64(tc.to))
			require.NoError(t, err)
			assert.Equal(t, len(tc.fetchedVersions), len(updates))

			for i, v := range tc.fetchedVersions {
				assert.Equal(t, int64(v), updates[i].Update.UpdateVersion)
			}
		})
	}
}

func TestDatastore_GetUpdateVersion(t *testing.T) {
	db, mds, cleanup := setupMDSTest(t)
	defer cleanup()

	err := db.Set(path.Join(topicVersionPrefix, "127.0.0.1"), "57")
	require.NoError(t, err)

	version, err := mds.GetUpdateVersion("127.0.0.1")
	require.NoError(t, err)
	assert.Equal(t, int64(57), version)
}

func TestDatastore_SetUpdateVersion(t *testing.T) {
	db, mds, cleanup := setupMDSTest(t)
	defer cleanup()

	err := mds.SetUpdateVersion("127.0.0.1", 123)
	require.NoError(t, err)

	savedVersion, err := db.Get(path.Join(topicVersionPrefix, "127.0.0.1"))
	require.NoError(t, err)
	i, err := strconv.ParseInt(string(savedVersion), 10, 64)
	require.NoError(t, err)
	assert.Equal(t, int64(123), i)
}

func TestDatastore_SetPodLabels(t *testing.T) {
	db, mds, cleanup := setupMDSTest(t)
	defer cleanup()

	err := mds.SetPodLabels("namespace1", "my_pod", map[string]string{"app": "my_app", "api/version": "v1"})
	require.NoError(t, err)

	val, err := db.Get(getNSLabelPodUpdateKey("namespace1", "app", "my_pod"))
	require.NoError(t, err)
	assert.Equal(t, "my_app", string(val))

	val, err = db.Get(getNSLabelPodUpdateKey("namespace1", "api/version", "my_pod"))
	require.NoError(t, err)
	assert.Equal(t, "v1", string(val))

	val, err = db.Get(getNSPodUpdateKey("namespace1", "my_pod"))
	require.NoError(t, err)
	labelKeys := make(map[string]bool)
	err = json.Unmarshal(val, &labelKeys)
	require.NoError(t, err)
	assert.True(t, reflect.DeepEqual(map[string]bool{"app": true, "api/version": true}, labelKeys))

	// Update labels for the pod.
	err = mds.SetPodLabels("namespace1", "my_pod", map[string]string{"api/version": "v2"})
	require.NoError(t, err)

	// Check that old label key `app` has been removed.
	val, err = db.Get(getNSLabelPodUpdateKey("namespace1", "app", "my_pod"))
	require.NoError(t, err)
	assert.Nil(t, val)

	// Check that label key `version` has been updated.
	val, err = db.Get(getNSLabelPodUpdateKey("namespace1", "api/version", "my_pod"))
	require.NoError(t, err)
	assert.Equal(t, "v2", string(val))
}

func TestDatastore_DeletePodLabels(t *testing.T) {
	db, mds, cleanup := setupMDSTest(t)
	defer cleanup()

	err := mds.SetPodLabels("namespace1", "my_pod", map[string]string{"app": "my_app", "api/version": "v1"})
	require.NoError(t, err)

	// Attempt to delete a pod with no labels.
	err = mds.DeletePodLabels("namespace1", "another_pod")
	require.NoError(t, err)
	val, err := db.Get(getNSLabelPodUpdateKey("namespace1", "app", "my_pod"))
	require.NoError(t, err)
	assert.Equal(t, "my_app", string(val))

	// Delete a pod with labels.
	err = mds.DeletePodLabels("namespace1", "my_pod")
	require.NoError(t, err)

	val, err = db.Get(getNSLabelPodUpdateKey("namespace1", "app", "my_pod"))
	require.NoError(t, err)
	assert.Nil(t, val)
	val, err = db.Get(getNSLabelPodUpdateKey("namespace1", "api/version", "my_pod"))
	require.NoError(t, err)
	assert.Nil(t, val)
	val, err = db.Get(getNSPodUpdateKey("namespace1", "my_pod"))
	require.NoError(t, err)
	assert.Nil(t, val)
}

func TestDataStore_FetchPodsWithLabelKey(t *testing.T) {
	_, mds, cleanup := setupMDSTest(t)
	defer cleanup()

	err := mds.SetPodLabels("namespace1", "pod1", map[string]string{"app": "my_app", "version": "v1"})
	require.NoError(t, err)
	err = mds.SetPodLabels("namespace1", "pod2", map[string]string{"app": "my_app", "version": "v2", "env": "prod"})
	require.NoError(t, err)

	pods, err := mds.FetchPodsWithLabelKey("namespace1", "app")
	require.NoError(t, err)
	assert.ElementsMatch(t, []string{"pod1", "pod2"}, pods)

	pods, err = mds.FetchPodsWithLabelKey("namespace1", "env")
	require.NoError(t, err)
	assert.ElementsMatch(t, []string{"pod2"}, pods)
}

func TestDataStore_FetchPodsWithLabels(t *testing.T) {
	_, mds, cleanup := setupMDSTest(t)
	defer cleanup()

	err := mds.SetPodLabels("namespace1", "pod1", map[string]string{"app": "my_app", "version": "v1"})
	require.NoError(t, err)
	err = mds.SetPodLabels("namespace1", "pod2", map[string]string{"app": "my_app", "version": "v2", "env": "prod"})
	require.NoError(t, err)
	err = mds.SetPodLabels("namespace1", "pod3", map[string]string{"app": "my_app", "version": "v1", "env": "staging"})
	require.NoError(t, err)

	pods, err := mds.FetchPodsWithLabels("namespace1", map[string]string{"app": "my_app", "version": "v1"})
	require.NoError(t, err)
	assert.ElementsMatch(t, []string{"pod1", "pod3"}, pods)

	pods, err = mds.FetchPodsWithLabels("namespace1", map[string]string{"env": "prod"})
	require.NoError(t, err)
	assert.ElementsMatch(t, []string{"pod2"}, pods)

	pods, err = mds.FetchPodsWithLabels("namespace1", map[string]string{"app": "my_app"})
	require.NoError(t, err)
	assert.ElementsMatch(t, []string{"pod1", "pod2", "pod3"}, pods)
}
