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

package k8s

import (
	"context"
	"io"
	"strings"
	"sync"
	"time"

	log "github.com/sirupsen/logrus"
	"k8s.io/apimachinery/pkg/api/errors"
	"k8s.io/apimachinery/pkg/api/meta"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/apimachinery/pkg/runtime"
	"k8s.io/apimachinery/pkg/util/sets"
	"k8s.io/cli-runtime/pkg/genericclioptions"
	"k8s.io/cli-runtime/pkg/printers"
	"k8s.io/cli-runtime/pkg/resource"
	"k8s.io/client-go/discovery"
	"k8s.io/client-go/discovery/cached/memory"
	"k8s.io/client-go/dynamic"
	"k8s.io/client-go/kubernetes"
	"k8s.io/client-go/rest"
	"k8s.io/client-go/restmapper"
	cmdutil "k8s.io/kubectl/pkg/cmd/util"
	cmdwait "k8s.io/kubectl/pkg/cmd/wait"
)

// ObjectDeleter has methods to delete K8s objects and wait for them. This code is adopted from `kubectl delete`.
type ObjectDeleter struct {
	Namespace  string
	Clientset  *kubernetes.Clientset
	RestConfig *rest.Config
	Timeout    time.Duration

	rcg           *restClientGetter
	dynamicClient dynamic.Interface
}

// DeleteCustomObject is used to delete a custom object (instantiation of CRD).
func (o *ObjectDeleter) DeleteCustomObject(resourceName, resourceValue string) error {
	if err := o.initRestClientGetter(); err != nil {
		return err
	}
	b := resource.NewBuilder(o.rcg)
	r := b.
		Unstructured().
		ContinueOnError().
		NamespaceParam(o.Namespace).
		ResourceNames(resourceName, resourceValue).
		RequireObject(false).
		Flatten().
		Do()

	err := r.Err()
	if err != nil {
		return err
	}
	if err := o.initDynamicClient(); err != nil {
		return err
	}

	_, err = o.runDelete(r)
	return err
}

// DeleteNamespace removes the namespace and all objects within it. Waits for deletion to complete.
func (o *ObjectDeleter) DeleteNamespace() error {
	if err := o.initRestClientGetter(); err != nil {
		return err
	}
	b := resource.NewBuilder(o.rcg)

	r := b.
		Unstructured().
		ContinueOnError().
		NamespaceParam(o.Namespace).
		ResourceNames("namespace", o.Namespace).
		RequireObject(false).
		Flatten().
		Do()

	err := r.Err()
	if err != nil {
		return err
	}
	if err := o.initDynamicClient(); err != nil {
		return err
	}

	_, err = o.runDelete(r)
	return err
}

func (o *ObjectDeleter) getDeletableResourceTypes() ([]string, error) {
	discoveryClient, err := o.rcg.ToDiscoveryClient()
	if err != nil {
		return nil, err
	}

	lists, err := discoveryClient.ServerPreferredResources()
	if err != nil {
		return nil, err
	}

	resources := []string{}
	for _, list := range lists {
		if len(list.APIResources) == 0 {
			continue
		}

		for _, resource := range list.APIResources {
			if len(resource.Verbs) == 0 {
				continue
			}
			if !sets.NewString(resource.Verbs...).HasAll("delete") {
				continue
			}
			resources = append(resources, resource.Name)
		}
	}
	return resources, nil
}

// DeleteByLabel delete objects that match the labels and specified by resourceKinds. Waits for deletion.
func (o *ObjectDeleter) DeleteByLabel(selector string, resourceKinds ...string) (int, error) {
	if err := o.initRestClientGetter(); err != nil {
		return 0, err
	}
	b := resource.NewBuilder(o.rcg)

	if len(resourceKinds) == 0 {
		allKinds, err := o.getDeletableResourceTypes()
		if err != nil {
			return 0, err
		}
		resourceKinds = allKinds
	}

	r := b.
		Unstructured().
		ContinueOnError().
		NamespaceParam(o.Namespace).
		LabelSelector(selector).
		ResourceTypeOrNameArgs(false, strings.Join(resourceKinds, ",")).
		RequireObject(false).
		Flatten().
		Do()

	err := r.Err()
	if err != nil {
		return 0, err
	}
	if err := o.initDynamicClient(); err != nil {
		return 0, err
	}

	return o.runDelete(r)
}

func (o *ObjectDeleter) runDelete(r *resource.Result) (int, error) {
	r = r.IgnoreErrors(errors.IsNotFound)
	deletedInfos := []*resource.Info{}
	uidMap := cmdwait.UIDMap{}
	found := 0
	err := r.Visit(func(info *resource.Info, err error) error {
		if err != nil {
			return err
		}
		deletedInfos = append(deletedInfos, info)
		found++

		options := metav1.NewDeleteOptions(0)
		policy := metav1.DeletePropagationBackground
		options.PropagationPolicy = &policy
		response, err := o.deleteResource(info, options)
		if err != nil {
			return err
		}
		resourceLocation := cmdwait.ResourceLocation{
			GroupResource: info.Mapping.Resource.GroupResource(),
			Namespace:     info.Namespace,
			Name:          info.Name,
		}
		if status, ok := response.(*metav1.Status); ok && status.Details != nil {
			uidMap[resourceLocation] = status.Details.UID
			return nil
		}
		responseMetadata, err := meta.Accessor(response)
		if err != nil {
			// We don't have UID, but we didn't fail the delete, next best thing is just skipping the UID.
			log.WithError(err).Trace("missing UID")
			return nil
		}
		uidMap[resourceLocation] = responseMetadata.GetUID()

		return nil
	})
	if err != nil {
		return 0, err
	}
	if found == 0 {
		return 0, nil
	}

	effectiveTimeout := o.Timeout
	if effectiveTimeout == 0 {
		// if we requested to wait forever, set it to a week.
		effectiveTimeout = 168 * time.Hour
	}
	waitOptions := cmdwait.WaitOptions{
		ResourceFinder: genericclioptions.ResourceFinderForResult(resource.InfoListVisitor(deletedInfos)),
		UIDMap:         uidMap,
		DynamicClient:  o.dynamicClient,
		Timeout:        effectiveTimeout,

		Printer:     printers.NewDiscardingPrinter(),
		ConditionFn: cmdwait.IsDeleted,
		IOStreams: genericclioptions.IOStreams{
			Out:    io.Discard,
			ErrOut: io.Discard,
		},
	}
	return found, waitOptions.RunWait()
}

func (o *ObjectDeleter) deleteResource(info *resource.Info, deleteOptions *metav1.DeleteOptions) (runtime.Object, error) {
	deleteResponse, err := resource.
		NewHelper(info.Client, info.Mapping).
		DeleteWithOptions(info.Namespace, info.Name, deleteOptions)

	if err != nil {
		return nil, cmdutil.AddSourceToErr("deleting", info.Source, err)
	}

	return deleteResponse, nil
}

func (o *ObjectDeleter) initRestClientGetter() error {
	if o.rcg != nil {
		return nil
	}
	o.rcg = &restClientGetter{
		clientset:  o.Clientset,
		restConfig: o.RestConfig,
	}
	return nil
}

func (o *ObjectDeleter) initDynamicClient() error {
	if o.dynamicClient != nil {
		return nil
	}
	config, err := o.rcg.ToRESTConfig()
	if err != nil {
		return err
	}
	c, err := dynamic.NewForConfig(config)
	if err != nil {
		return err
	}
	o.dynamicClient = c
	return nil
}

// DeleteClusterRole deletes the clusterrole with the given name.
func DeleteClusterRole(clientset kubernetes.Interface, name string) error {
	crs := clientset.RbacV1().ClusterRoles()
	err := crs.Delete(context.Background(), name, metav1.DeleteOptions{})
	if err != nil {
		return err
	}

	return nil
}

// DeleteClusterRoleBinding deletes the clusterrolebinding with the given name.
func DeleteClusterRoleBinding(clientset kubernetes.Interface, name string) error {
	crbs := clientset.RbacV1().ClusterRoleBindings()

	err := crbs.Delete(context.Background(), name, metav1.DeleteOptions{})
	if err != nil {
		return err
	}

	return nil
}

// DeleteConfigMap deletes the config map in the namespace with the given name.
func DeleteConfigMap(clientset kubernetes.Interface, name string, namespace string) error {
	cm := clientset.CoreV1().ConfigMaps(namespace)

	err := cm.Delete(context.Background(), name, metav1.DeleteOptions{})
	if err != nil {
		return err
	}

	return nil
}

// DeleteAllResources deletes all resources in the given namespace with the given selector.
func DeleteAllResources(clientset kubernetes.Interface, ns string, selectors string) error {
	err := DeleteDeployments(clientset, ns, selectors)
	if err != nil {
		return err
	}

	err = DeleteDaemonSets(clientset, ns, selectors)
	if err != nil {
		return err
	}

	err = DeleteServices(clientset, ns, selectors)
	if err != nil {
		return err
	}

	err = DeletePods(clientset, ns, selectors)
	if err != nil {
		return err
	}

	return nil
}

// DeleteDeployments deletes all deployments in the namespace with the given selector.
func DeleteDeployments(clientset kubernetes.Interface, namespace string, selectors string) error {
	deployments := clientset.AppsV1().Deployments(namespace)

	if err := deployments.DeleteCollection(context.Background(), metav1.DeleteOptions{}, metav1.ListOptions{LabelSelector: selectors}); err != nil {
		return err
	}
	return nil
}

// DeleteDaemonSets deletes all daemonsets in the namespace with the given selector.
func DeleteDaemonSets(clientset kubernetes.Interface, namespace string, selectors string) error {
	daemonsets := clientset.AppsV1().DaemonSets(namespace)

	if err := daemonsets.DeleteCollection(context.Background(), metav1.DeleteOptions{}, metav1.ListOptions{LabelSelector: selectors}); err != nil {
		return err
	}
	return nil
}

// DeleteServices deletes all services in the namespace with the given selector.
func DeleteServices(clientset kubernetes.Interface, namespace string, selectors string) error {
	svcs := clientset.CoreV1().Services(namespace)

	l, err := svcs.List(context.Background(), metav1.ListOptions{LabelSelector: selectors})
	if err != nil {
		return err
	}
	for _, s := range l.Items {
		err = svcs.Delete(context.Background(), s.ObjectMeta.Name, metav1.DeleteOptions{})
		if err != nil {
			return err
		}
	}

	return nil
}

// DeletePods deletes all pods in the namespace with the given selector.
func DeletePods(clientset kubernetes.Interface, namespace string, selectors string) error {
	pods := clientset.CoreV1().Pods(namespace)

	l, err := pods.List(context.Background(), metav1.ListOptions{LabelSelector: selectors})
	if err != nil {
		return err
	}
	for _, s := range l.Items {
		err = pods.Delete(context.Background(), s.ObjectMeta.Name, metav1.DeleteOptions{})
		if err != nil {
			return err
		}
	}

	return nil
}

type restClientGetter struct {
	clientset  *kubernetes.Clientset
	restConfig *rest.Config

	discoveryClientLock sync.Mutex
	discoveryClient     discovery.CachedDiscoveryInterface
}

func (r *restClientGetter) ToRESTConfig() (*rest.Config, error) {
	return r.restConfig, nil
}

func (r *restClientGetter) ToDiscoveryClient() (discovery.CachedDiscoveryInterface, error) {
	r.discoveryClientLock.Lock()
	defer r.discoveryClientLock.Unlock()
	if r.discoveryClient == nil {
		c, err := r.toDiscoveryClient()
		if err != nil {
			return nil, err
		}
		r.discoveryClient = c
	}
	return r.discoveryClient, nil
}

const (
	kubectlDefaultBurst = 300
	kubectlDefaultQPS   = 50.0
)

func (r *restClientGetter) toDiscoveryClient() (discovery.CachedDiscoveryInterface, error) {
	config, err := r.ToRESTConfig()
	if err != nil {
		return nil, err
	}

	config.Burst = kubectlDefaultBurst
	config.QPS = kubectlDefaultQPS

	discoveryClient, err := discovery.NewDiscoveryClientForConfig(config)
	if err != nil {
		return nil, err
	}

	return memory.NewMemCacheClient(discoveryClient), nil
}

func (r *restClientGetter) ToRESTMapper() (meta.RESTMapper, error) {
	discoveryClient, err := r.ToDiscoveryClient()
	if err != nil {
		return nil, err
	}
	apiGroupResources, err := restmapper.GetAPIGroupResources(discoveryClient)
	if err != nil {
		return nil, err
	}
	return restmapper.NewDiscoveryRESTMapper(apiGroupResources), nil
}
