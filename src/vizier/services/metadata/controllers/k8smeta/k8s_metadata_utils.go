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
	"sort"

	log "github.com/sirupsen/logrus"
	apps "k8s.io/api/apps/v1"
	v1 "k8s.io/api/core/v1"
	discoveryv1 "k8s.io/api/discovery/v1"
	"k8s.io/apimachinery/pkg/labels"
	"k8s.io/apimachinery/pkg/watch"
	"k8s.io/client-go/informers"
	discoverylisters "k8s.io/client-go/listers/discovery/v1"
	"k8s.io/client-go/tools/cache"

	"px.dev/pixie/src/shared/k8s"
	"px.dev/pixie/src/vizier/services/metadata/storepb"
)

func createHandlers(convert func(obj interface{}) *K8sResourceMessage, ch chan *K8sResourceMessage) cache.ResourceEventHandler {
	return cache.ResourceEventHandlerFuncs{
		AddFunc: func(obj interface{}) {
			msg := convert(obj)
			if msg != nil {
				msg.EventType = watch.Added
				ch <- msg
			}
		},
		UpdateFunc: func(oldObj, newObj interface{}) {
			msg := convert(newObj)
			if msg != nil {
				msg.EventType = watch.Modified
				ch <- msg
			}
		},
		DeleteFunc: func(obj interface{}) {
			msg := convert(obj)
			if msg != nil {
				msg.EventType = watch.Deleted
				ch <- msg
			}
		},
	}
}

func startNodeWatcher(ch chan *K8sResourceMessage, quitCh <-chan struct{}, factory informers.SharedInformerFactory) {
	nodes := factory.Core().V1().Nodes()

	inf := nodes.Informer()
	_, _ = inf.AddEventHandler(createHandlers(nodeConverter, ch))
	go inf.Run(quitCh)

	cache.WaitForCacheSync(quitCh, inf.HasSynced)

	// A cache sync doesn't guarantee that the handlers have been called,
	// so instead we manually list and call the Add handlers since subsequent
	// resources depend on these.
	list, err := nodes.Lister().List(labels.Everything())
	if err != nil {
		log.WithError(err).Errorf("Failed to init nodes")
	}

	for i := range list {
		msg := nodeConverter(list[i])
		if msg != nil {
			msg.EventType = watch.Added
			ch <- msg
		}
	}
}

func startNamespaceWatcher(ch chan *K8sResourceMessage, quitCh <-chan struct{}, factory informers.SharedInformerFactory) {
	inf := factory.Core().V1().Namespaces().Informer()
	_, _ = inf.AddEventHandler(createHandlers(namespaceConverter, ch))
	go inf.Run(quitCh)
}

func startPodWatcher(ch chan *K8sResourceMessage, quitCh <-chan struct{}, factories []informers.SharedInformerFactory) {
	for _, factory := range factories {
		pods := factory.Core().V1().Pods()

		inf := pods.Informer()
		_, _ = inf.AddEventHandler(createHandlers(podConverter, ch))
		go inf.Run(quitCh)

		cache.WaitForCacheSync(quitCh, inf.HasSynced)

		// A cache sync doesn't guarantee that the handlers have been called,
		// so instead we manually list and call the Add handlers since subsequent
		// resources depend on these.
		list, err := pods.Lister().List(labels.Everything())
		if err != nil {
			log.WithError(err).Errorf("Failed to init pods")
		}

		for i := range list {
			msg := podConverter(list[i])
			if msg != nil {
				msg.EventType = watch.Added
				ch <- msg
			}
		}
	}
}

func startServiceWatcher(ch chan *K8sResourceMessage, quitCh <-chan struct{}, factories []informers.SharedInformerFactory) {
	for _, factory := range factories {
		inf := factory.Core().V1().Services().Informer()
		_, _ = inf.AddEventHandler(createHandlers(serviceConverter, ch))
		go inf.Run(quitCh)
	}
}

func startEndpointsWatcher(ch chan *K8sResourceMessage, quitCh <-chan struct{}, factories []informers.SharedInformerFactory) {
	for _, factory := range factories {
		slices := factory.Discovery().V1().EndpointSlices()
		inf := slices.Informer()
		_, _ = inf.AddEventHandler(createEndpointSliceHandlers(slices.Lister(), ch))
		go inf.Run(quitCh)
	}
}

// createEndpointSliceHandlers sends an endpoints update for the whole Service whenever one of
// its EndpointSlices changes, so the rest of the metadata service keeps seeing one Endpoints
// object per Service like it did with the v1 Endpoints API.
func createEndpointSliceHandlers(lister discoverylisters.EndpointSliceLister, ch chan *K8sResourceMessage) cache.ResourceEventHandler {
	send := func(obj interface{}, eventType watch.EventType) {
		if tombstone, ok := obj.(cache.DeletedFinalStateUnknown); ok {
			obj = tombstone.Obj
		}
		slice, ok := obj.(*discoveryv1.EndpointSlice)
		if !ok {
			return
		}
		msg, lastSlice := endpointSlicesConverter(lister, slice, eventType == watch.Deleted)
		if msg == nil {
			return
		}
		msg.EventType = eventType
		if eventType == watch.Deleted && !lastSlice {
			// The Service still has other slices, so this is an update, not a delete.
			msg.EventType = watch.Modified
		}
		ch <- msg
	}
	return cache.ResourceEventHandlerFuncs{
		AddFunc:    func(obj interface{}) { send(obj, watch.Added) },
		UpdateFunc: func(_, newObj interface{}) { send(newObj, watch.Modified) },
		DeleteFunc: func(obj interface{}) { send(obj, watch.Deleted) },
	}
}

func startReplicaSetWatcher(ch chan *K8sResourceMessage, quitCh <-chan struct{}, factories []informers.SharedInformerFactory) {
	for _, factory := range factories {
		inf := factory.Apps().V1().ReplicaSets().Informer()
		_, _ = inf.AddEventHandler(createHandlers(replicaSetConverter, ch))
		go inf.Run(quitCh)
	}
}

func startDeploymentWatcher(ch chan *K8sResourceMessage, quitCh <-chan struct{}, factories []informers.SharedInformerFactory) {
	for _, factory := range factories {
		inf := factory.Apps().V1().Deployments().Informer()
		_, _ = inf.AddEventHandler(createHandlers(deploymentConverter, ch))
		go inf.Run(quitCh)
	}
}

func podConverter(obj interface{}) *K8sResourceMessage {
	return &K8sResourceMessage{
		ObjectType: "pods",
		Object: &storepb.K8SResource{
			Resource: &storepb.K8SResource_Pod{
				Pod: k8s.PodToProto(obj.(*v1.Pod)),
			},
		},
	}
}

func serviceConverter(obj interface{}) *K8sResourceMessage {
	return &K8sResourceMessage{
		ObjectType: "services",
		Object: &storepb.K8SResource{
			Resource: &storepb.K8SResource_Service{
				Service: k8s.ServiceToProto(obj.(*v1.Service)),
			},
		},
	}
}

func namespaceConverter(obj interface{}) *K8sResourceMessage {
	return &K8sResourceMessage{
		ObjectType: "namespaces",
		Object: &storepb.K8SResource{
			Resource: &storepb.K8SResource_Namespace{
				Namespace: k8s.NamespaceToProto(obj.(*v1.Namespace)),
			},
		},
	}
}

// endpointSlicesConverter builds the endpoints message for the Service that owns the given slice
// from all of that Service's slices in the informer cache. When the deleted slice was the last
// one, the message is built from it so the delete still carries the last known addresses.
func endpointSlicesConverter(lister discoverylisters.EndpointSliceLister, slice *discoveryv1.EndpointSlice, deleted bool) (*K8sResourceMessage, bool) {
	serviceName := slice.Labels[discoveryv1.LabelServiceName]
	if serviceName == "" {
		return nil, false
	}
	selector := labels.SelectorFromSet(labels.Set{discoveryv1.LabelServiceName: serviceName})
	slices, err := lister.EndpointSlices(slice.Namespace).List(selector)
	if err != nil {
		log.WithError(err).Error("Failed to list EndpointSlices")
		return nil, false
	}
	deletedLast := false
	if deleted && len(slices) == 0 {
		slices = []*discoveryv1.EndpointSlice{slice}
		deletedLast = true
	}
	sort.Slice(slices, func(i, j int) bool {
		// Prefer IPv4 addresses for dual-stack services, then keep the order stable.
		if slices[i].AddressType != slices[j].AddressType {
			return slices[i].AddressType == discoveryv1.AddressTypeIPv4
		}
		return slices[i].Name < slices[j].Name
	})

	return &K8sResourceMessage{
		ObjectType: "endpoints",
		Object: &storepb.K8SResource{
			Resource: &storepb.K8SResource_Endpoints{
				Endpoints: k8s.EndpointSlicesToProto(serviceName, slices),
			},
		},
	}, deletedLast
}

func nodeConverter(obj interface{}) *K8sResourceMessage {
	return &K8sResourceMessage{
		ObjectType: "nodes",
		Object: &storepb.K8SResource{
			Resource: &storepb.K8SResource_Node{
				Node: k8s.NodeToProto(obj.(*v1.Node)),
			},
		},
	}
}

func replicaSetConverter(obj interface{}) *K8sResourceMessage {
	return &K8sResourceMessage{
		ObjectType: "replicasets",
		Object: &storepb.K8SResource{
			Resource: &storepb.K8SResource_ReplicaSet{
				ReplicaSet: k8s.ReplicaSetToProto(obj.(*apps.ReplicaSet)),
			},
		},
	}
}

func deploymentConverter(obj interface{}) *K8sResourceMessage {
	return &K8sResourceMessage{
		ObjectType: "deployments",
		Object: &storepb.K8SResource{
			Resource: &storepb.K8SResource_Deployment{
				Deployment: k8s.DeploymentToProto(obj.(*apps.Deployment)),
			},
		},
	}
}
