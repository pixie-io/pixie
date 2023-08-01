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
	log "github.com/sirupsen/logrus"
	apps "k8s.io/api/apps/v1"
	v1 "k8s.io/api/core/v1"
	"k8s.io/apimachinery/pkg/labels"
	"k8s.io/apimachinery/pkg/watch"
	"k8s.io/client-go/informers"
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
		inf := factory.Core().V1().Endpoints().Informer()
		_, _ = inf.AddEventHandler(createHandlers(endpointsConverter, ch))
		go inf.Run(quitCh)
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

func endpointsConverter(obj interface{}) *K8sResourceMessage {
	return &K8sResourceMessage{
		ObjectType: "endpoints",
		Object: &storepb.K8SResource{
			Resource: &storepb.K8SResource_Endpoints{
				Endpoints: k8s.EndpointsToProto(obj.(*v1.Endpoints)),
			},
		},
	}
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
