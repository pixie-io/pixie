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
	v1 "k8s.io/api/core/v1"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/apimachinery/pkg/watch"

	"px.dev/pixie/src/shared/k8s/metadatapb"
	"px.dev/pixie/src/vizier/services/metadata/storepb"
)

// This tests just the pod watcher, since the generated code for all of the watchers are
// the same, minus substitutions for the object types.
func TestPodWatcher_SyncPodImpl(t *testing.T) {
	// Protos stored in the datastore.
	// This pod is currently running in k8s.
	alivePod := &storepb.K8SResource{
		Resource: &storepb.K8SResource_Pod{
			Pod: &metadatapb.Pod{
				Metadata: &metadatapb.ObjectMetadata{
					Name:                "alive_pod",
					UID:                 "1234",
					ResourceVersion:     "1",
					ClusterName:         "a_cluster",
					CreationTimestampNS: 4,
					DeletionTimestampNS: 0,
				},
			},
		},
	}
	// This pod is running, but will die before MDS stops.
	dyingPod1 := &storepb.K8SResource{
		Resource: &storepb.K8SResource_Pod{
			Pod: &metadatapb.Pod{
				Metadata: &metadatapb.ObjectMetadata{
					Name:                "dying_pod",
					UID:                 "1238",
					ResourceVersion:     "2",
					ClusterName:         "a_cluster",
					CreationTimestampNS: 4,
					DeletionTimestampNS: 0,
				},
			},
		},
	}
	dyingPod2 := &storepb.K8SResource{
		Resource: &storepb.K8SResource_Pod{
			Pod: &metadatapb.Pod{
				Metadata: &metadatapb.ObjectMetadata{
					Name:                "dying_pod",
					UID:                 "1238",
					ResourceVersion:     "4",
					ClusterName:         "a_cluster",
					CreationTimestampNS: 4,
					DeletionTimestampNS: 5,
				},
			},
		},
	}
	// This pod had died when MDS was still running. It is already terminated,
	// so we don't need to send another termination event for it.
	deadPod := &storepb.K8SResource{
		Resource: &storepb.K8SResource_Pod{
			Pod: &metadatapb.Pod{
				Metadata: &metadatapb.ObjectMetadata{
					Name:                "dead_pod",
					UID:                 "1235",
					ResourceVersion:     "5",
					ClusterName:         "a_cluster",
					CreationTimestampNS: 5,
					DeletionTimestampNS: 6,
				},
			},
		},
	}
	// This pod was running when MDS was last running, but is no longer running
	// in the cluster. A termination event should be sent.
	zombiePod := &storepb.K8SResource{
		Resource: &storepb.K8SResource_Pod{
			Pod: &metadatapb.Pod{
				Metadata: &metadatapb.ObjectMetadata{
					Name:                "zombie_pod",
					UID:                 "1236",
					ResourceVersion:     "6",
					ClusterName:         "a_cluster",
					CreationTimestampNS: 5,
					DeletionTimestampNS: 0,
				},
			},
		},
	}
	storedUpdates := []*storepb.K8SResource{alivePod, dyingPod1, dyingPod2, deadPod, zombiePod}

	// These are all pods that are currently running in K8s, at the time
	// of MDS starting up.
	creationTime := metav1.Unix(0, 4)
	newPod := v1.Pod{
		ObjectMeta: metav1.ObjectMeta{
			Name:              "current_pod",
			UID:               "2000",
			ResourceVersion:   "4",
			ClusterName:       "a_cluster",
			CreationTimestamp: creationTime,
		},
	}
	existingPod := v1.Pod{
		ObjectMeta: metav1.ObjectMeta{
			Name:              "alive_pod",
			UID:               "1234",
			ResourceVersion:   "5",
			ClusterName:       "a_cluster",
			CreationTimestamp: creationTime,
		},
	}
	currentState := &v1.PodList{
		Items: []v1.Pod{newPod, existingPod},
	}

	updateCh := make(chan *K8sResourceMessage, 10)
	watcher := NewPodWatcher("pods", updateCh, nil)

	watcher.syncPodImpl(storedUpdates, currentState)

	expectedUpdates := []*metadatapb.ObjectMetadata{
		{
			Name:                "current_pod",
			UID:                 "2000",
			ResourceVersion:     "4",
			ClusterName:         "a_cluster",
			CreationTimestampNS: 4,
			DeletionTimestampNS: 0,
			OwnerReferences:     []*metadatapb.OwnerReference{},
		},
		{
			Name:                "alive_pod",
			UID:                 "1234",
			ResourceVersion:     "5",
			ClusterName:         "a_cluster",
			CreationTimestampNS: 4,
			DeletionTimestampNS: 0,
			OwnerReferences:     []*metadatapb.OwnerReference{},
		},
		{
			Name:                "zombie_pod",
			UID:                 "1236",
			ResourceVersion:     "6",
			ClusterName:         "a_cluster",
			CreationTimestampNS: 5,
			DeletionTimestampNS: 0,
		},
	}
	expectedEventTypes := []watch.EventType{
		watch.Modified,
		watch.Modified,
		watch.Deleted,
	}

	assert.Equal(t, 3, len(updateCh))
	for i := range expectedUpdates {
		update := <-updateCh
		assert.Equal(t, expectedUpdates[i], update.Object.GetPod().Metadata)
		assert.Equal(t, expectedEventTypes[i], update.EventType)
	}
}
