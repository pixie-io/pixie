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
	"context"
	"time"

	log "github.com/sirupsen/logrus"
	apps "k8s.io/api/apps/v1"
	v1 "k8s.io/api/core/v1"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/apimachinery/pkg/watch"
	"k8s.io/client-go/informers"
	"k8s.io/client-go/kubernetes"
	"k8s.io/client-go/tools/cache"

	"px.dev/pixie/src/shared/k8s"
	"px.dev/pixie/src/vizier/services/metadata/storepb"
)

type informerWatcher struct {
	convert   func(obj interface{}) *K8sResourceMessage
	ch        chan *K8sResourceMessage
	init      func() error
	informers []cache.SharedIndexInformer
}

// StartWatcher starts a watcher.
func (i *informerWatcher) StartWatcher(quitCh chan struct{}) {
	for _, inf := range i.informers {
		_, _ = inf.AddEventHandler(cache.ResourceEventHandlerFuncs{
			AddFunc: func(obj interface{}) {
				msg := i.convert(obj)
				if msg != nil {
					msg.EventType = watch.Added
					i.ch <- msg
				}
			},
			UpdateFunc: func(oldObj, newObj interface{}) {
				msg := i.convert(newObj)
				if msg != nil {
					msg.EventType = watch.Modified
					i.ch <- msg
				}
			},
			DeleteFunc: func(obj interface{}) {
				msg := i.convert(obj)
				if msg != nil {
					msg.EventType = watch.Deleted
					i.ch <- msg
				}
			},
		})
		inf.Run(quitCh)
	}
}

// InitWatcher initializes a watcher, for example to perform a list.
func (i *informerWatcher) InitWatcher() error {
	if i.init != nil {
		return i.init()
	}
	return nil
}

func podWatcher(namespaces []string, ch chan *K8sResourceMessage, clientset kubernetes.Interface) *informerWatcher {
	iw := &informerWatcher{
		convert: podConverter,
		ch:      ch,
	}

	for _, ns := range namespaces {
		factory := informers.NewSharedInformerFactoryWithOptions(clientset, 12*time.Hour, informers.WithNamespace(ns))
		inf := factory.Core().V1().Pods().Informer()
		iw.informers = append(iw.informers, inf)
	}

	init := func() error {
		var podList []v1.Pod
		// We initialize ch with the current Pods to handle cold start race conditions.
		for _, ns := range namespaces {
			list, err := clientset.CoreV1().Pods(ns).List(context.Background(), metav1.ListOptions{})
			if err != nil {
				log.WithError(err).Errorf("Failed to init pods in %s namespace.", ns)
				return err
			}
			podList = append(podList, list.Items...)
		}

		for _, obj := range podList {
			item := obj
			msg := iw.convert(&item)
			if msg != nil {
				msg.EventType = watch.Added
				iw.ch <- msg
			}
		}
		return nil
	}

	iw.init = init

	return iw
}

func serviceWatcher(namespaces []string, ch chan *K8sResourceMessage, clientset kubernetes.Interface) *informerWatcher {
	iw := &informerWatcher{
		convert: serviceConverter,
		ch:      ch,
	}

	for _, ns := range namespaces {
		factory := informers.NewSharedInformerFactoryWithOptions(clientset, 12*time.Hour, informers.WithNamespace(ns))
		inf := factory.Core().V1().Services().Informer()
		iw.informers = append(iw.informers, inf)
	}

	return iw
}

func namespaceWatcher(namespaces []string, ch chan *K8sResourceMessage, clientset kubernetes.Interface) *informerWatcher {
	iw := &informerWatcher{
		convert: namespaceConverter,
		ch:      ch,
	}

	for _, ns := range namespaces {
		factory := informers.NewSharedInformerFactoryWithOptions(clientset, 12*time.Hour, informers.WithNamespace(ns))
		inf := factory.Core().V1().Namespaces().Informer()
		iw.informers = append(iw.informers, inf)
	}

	return iw
}

func endpointsWatcher(namespaces []string, ch chan *K8sResourceMessage, clientset kubernetes.Interface) *informerWatcher {
	iw := &informerWatcher{
		convert: endpointsConverter,
		ch:      ch,
	}

	for _, ns := range namespaces {
		factory := informers.NewSharedInformerFactoryWithOptions(clientset, 12*time.Hour, informers.WithNamespace(ns))
		inf := factory.Core().V1().Endpoints().Informer()
		iw.informers = append(iw.informers, inf)
	}

	return iw
}

func nodeWatcher(namespaces []string, ch chan *K8sResourceMessage, clientset kubernetes.Interface) *informerWatcher {
	iw := &informerWatcher{
		convert: nodeConverter,
		ch:      ch,
	}

	for _, ns := range namespaces {
		factory := informers.NewSharedInformerFactoryWithOptions(clientset, 12*time.Hour, informers.WithNamespace(ns))
		inf := factory.Core().V1().Nodes().Informer()
		iw.informers = append(iw.informers, inf)
	}

	init := func() error {
		// We initialize ch with the current nodes to handle cold start race conditions.
		list, err := clientset.CoreV1().Nodes().List(context.Background(), metav1.ListOptions{})
		if err != nil {
			return err
		}

		for _, obj := range list.Items {
			item := obj
			msg := iw.convert(&item)
			if msg != nil {
				msg.EventType = watch.Added
				iw.ch <- msg
			}
		}
		return nil
	}

	iw.init = init

	return iw
}

func replicaSetWatcher(namespaces []string, ch chan *K8sResourceMessage, clientset kubernetes.Interface) *informerWatcher {
	iw := &informerWatcher{
		convert: replicaSetConverter,
		ch:      ch,
	}

	for _, ns := range namespaces {
		factory := informers.NewSharedInformerFactoryWithOptions(clientset, 12*time.Hour, informers.WithNamespace(ns))
		inf := factory.Apps().V1().ReplicaSets().Informer()
		iw.informers = append(iw.informers, inf)
	}

	return iw
}

func deploymentWatcher(namespaces []string, ch chan *K8sResourceMessage, clientset kubernetes.Interface) *informerWatcher {
	iw := &informerWatcher{
		convert: deploymentConverter,
		ch:      ch,
	}

	for _, ns := range namespaces {
		factory := informers.NewSharedInformerFactoryWithOptions(clientset, 12*time.Hour, informers.WithNamespace(ns))
		inf := factory.Apps().V1().Deployments().Informer()
		iw.informers = append(iw.informers, inf)
	}

	return iw
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
