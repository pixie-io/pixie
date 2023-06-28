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
	convert func(obj interface{}) *K8sResourceMessage
	objType string
	ch      chan *K8sResourceMessage
	inf     cache.SharedIndexInformer
}

func (i *informerWatcher) send(msg *K8sResourceMessage, et watch.EventType) {
	msg.ObjectType = i.objType
	msg.EventType = et

	i.ch <- msg
}

// StartWatcher starts a watcher.
func (i *informerWatcher) StartWatcher(quitCh chan struct{}) {
	_, _ = i.inf.AddEventHandler(cache.ResourceEventHandlerFuncs{
		AddFunc: func(obj interface{}) {
			msg := i.convert(obj)
			if msg != nil {
				i.send(msg, watch.Added)
			}
		},
		UpdateFunc: func(oldObj, newObj interface{}) {
			msg := i.convert(newObj)
			if msg != nil {
				i.send(msg, watch.Modified)
			}
		},
		DeleteFunc: func(obj interface{}) {
			msg := i.convert(obj)
			if msg != nil {
				i.send(msg, watch.Deleted)
			}
		},
	})
	i.inf.Run(quitCh)
}

func podWatcher(resource string, ch chan *K8sResourceMessage, clientset kubernetes.Interface) *informerWatcher {
	factory := informers.NewSharedInformerFactory(clientset, 12*time.Hour)
	iw := &informerWatcher{
		convert: podConverter,
		objType: resource,
		ch:      ch,
		inf:     factory.Core().V1().Pods().Informer(),
	}
	// We initialize ch with the current nodes to handle cold start race conditions.
	list, err := clientset.CoreV1().Pods(v1.NamespaceAll).List(context.Background(), metav1.ListOptions{})
	if err != nil {
		log.WithError(err).Errorf("Failed to init %s", resource)
		// Still return the informer because the rest of the system can recover from this.
		return iw
	}

	for _, obj := range list.Items {
		item := obj
		msg := iw.convert(&item)
		if msg != nil {
			iw.send(msg, watch.Added)
		}
	}

	return iw
}

func serviceWatcher(resource string, ch chan *K8sResourceMessage, clientset kubernetes.Interface) *informerWatcher {
	factory := informers.NewSharedInformerFactory(clientset, 12*time.Hour)
	return &informerWatcher{
		convert: serviceConverter,
		objType: resource,
		ch:      ch,
		inf:     factory.Core().V1().Services().Informer(),
	}
}

func namespaceWatcher(resource string, ch chan *K8sResourceMessage, clientset kubernetes.Interface) *informerWatcher {
	factory := informers.NewSharedInformerFactory(clientset, 12*time.Hour)
	return &informerWatcher{
		convert: namespaceConverter,
		objType: resource,
		ch:      ch,
		inf:     factory.Core().V1().Namespaces().Informer(),
	}
}

func endpointsWatcher(resource string, ch chan *K8sResourceMessage, clientset kubernetes.Interface) *informerWatcher {
	factory := informers.NewSharedInformerFactory(clientset, 12*time.Hour)
	return &informerWatcher{
		convert: endpointsConverter,
		objType: resource,
		ch:      ch,
		inf:     factory.Core().V1().Endpoints().Informer(),
	}
}

func nodeWatcher(resource string, ch chan *K8sResourceMessage, clientset kubernetes.Interface) *informerWatcher {
	factory := informers.NewSharedInformerFactory(clientset, 12*time.Hour)
	iw := &informerWatcher{
		convert: nodeConverter,
		objType: resource,
		ch:      ch,
		inf:     factory.Core().V1().Nodes().Informer(),
	}

	// We initialize ch with the current nodes to handle cold start race conditions.
	list, err := clientset.CoreV1().Nodes().List(context.Background(), metav1.ListOptions{})
	if err != nil {
		log.WithError(err).Errorf("Failed to init %s", resource)
		// Still return the informer because the rest of the system can recover from this.
		return iw
	}

	for _, obj := range list.Items {
		item := obj
		msg := iw.convert(&item)
		if msg != nil {
			iw.send(msg, watch.Added)
		}
	}
	return iw
}

func replicaSetWatcher(resource string, ch chan *K8sResourceMessage, clientset kubernetes.Interface) *informerWatcher {
	factory := informers.NewSharedInformerFactory(clientset, 12*time.Hour)
	return &informerWatcher{
		convert: replicaSetConverter,
		objType: resource,
		ch:      ch,
		inf:     factory.Apps().V1().ReplicaSets().Informer(),
	}
}

func deploymentWatcher(resource string, ch chan *K8sResourceMessage, clientset kubernetes.Interface) *informerWatcher {
	factory := informers.NewSharedInformerFactory(clientset, 12*time.Hour)
	return &informerWatcher{
		convert: deploymentConverter,
		objType: resource,
		ch:      ch,
		inf:     factory.Apps().V1().Deployments().Informer(),
	}
}

func podConverter(obj interface{}) *K8sResourceMessage {
	o, ok := obj.(*v1.Pod)
	if !ok {
		return nil
	}

	return &K8sResourceMessage{
		Object: &storepb.K8SResource{
			Resource: &storepb.K8SResource_Pod{
				Pod: k8s.PodToProto(o),
			},
		},
	}
}

func serviceConverter(obj interface{}) *K8sResourceMessage {
	o, ok := obj.(*v1.Service)
	if !ok {
		return nil
	}

	return &K8sResourceMessage{
		Object: &storepb.K8SResource{
			Resource: &storepb.K8SResource_Service{
				Service: k8s.ServiceToProto(o),
			},
		},
	}
}

func namespaceConverter(obj interface{}) *K8sResourceMessage {
	o, ok := obj.(*v1.Namespace)
	if !ok {
		return nil
	}

	return &K8sResourceMessage{
		Object: &storepb.K8SResource{
			Resource: &storepb.K8SResource_Namespace{
				Namespace: k8s.NamespaceToProto(o),
			},
		},
	}
}

func endpointsConverter(obj interface{}) *K8sResourceMessage {
	o, ok := obj.(*v1.Endpoints)
	if !ok {
		return nil
	}

	return &K8sResourceMessage{
		Object: &storepb.K8SResource{
			Resource: &storepb.K8SResource_Endpoints{
				Endpoints: k8s.EndpointsToProto(o),
			},
		},
	}
}

func nodeConverter(obj interface{}) *K8sResourceMessage {
	o, ok := obj.(*v1.Node)
	if !ok {
		return nil
	}

	return &K8sResourceMessage{
		Object: &storepb.K8SResource{
			Resource: &storepb.K8SResource_Node{
				Node: k8s.NodeToProto(o),
			},
		},
	}
}

func replicaSetConverter(obj interface{}) *K8sResourceMessage {
	o, ok := obj.(*apps.ReplicaSet)
	if !ok {
		return nil
	}

	return &K8sResourceMessage{
		Object: &storepb.K8SResource{
			Resource: &storepb.K8SResource_ReplicaSet{
				ReplicaSet: k8s.ReplicaSetToProto(o),
			},
		},
	}
}

func deploymentConverter(obj interface{}) *K8sResourceMessage {
	o, ok := obj.(*apps.Deployment)
	if !ok {
		return nil
	}

	return &K8sResourceMessage{
		Object: &storepb.K8SResource{
			Resource: &storepb.K8SResource_Deployment{
				Deployment: k8s.DeploymentToProto(o),
			},
		},
	}
}
