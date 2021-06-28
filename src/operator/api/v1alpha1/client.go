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

package v1alpha1

import (
	"context"

	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/apimachinery/pkg/runtime/serializer"
	"k8s.io/client-go/kubernetes/scheme"
	"k8s.io/client-go/rest"
)

// VizierClient is a k8s API client for accessing the Vizier resource.
// +kubebuilder:object:generate=false
type VizierClient struct {
	client *rest.RESTClient
}

// NewVizierClient creates a new k8s API client which can acccess the Vizier resource.
func NewVizierClient(config *rest.Config) (*VizierClient, error) {
	err := AddToScheme(scheme.Scheme)
	if err != nil {
		return nil, err
	}

	crdConfig := *config
	crdConfig.ContentConfig.GroupVersion = &GroupVersion
	crdConfig.APIPath = "/apis"
	crdConfig.NegotiatedSerializer = serializer.NewCodecFactory(scheme.Scheme)
	crdConfig.UserAgent = rest.DefaultKubernetesUserAgent()
	crdClient, err := rest.UnversionedRESTClientFor(&crdConfig)
	if err != nil {
		return nil, err
	}

	return &VizierClient{client: crdClient}, nil
}

// List lists all Vizier resources in the given namespace.
func (c *VizierClient) List(ctx context.Context, namespace string, opts metav1.ListOptions) (*VizierList, error) {
	result := &VizierList{}
	err := c.client.Get().
		Namespace(namespace).
		Resource("viziers").
		VersionedParams(&opts, scheme.ParameterCodec).
		Do(ctx).
		Into(result)
	return result, err
}

// Update updates the Vizier resource.
func (c *VizierClient) Update(ctx context.Context, vz *Vizier, namespace string, opts metav1.UpdateOptions) (*Vizier, error) {
	result := &Vizier{}
	err := c.client.Put().
		Namespace(namespace).
		Resource("viziers").
		Name(vz.Name).
		VersionedParams(&opts, scheme.ParameterCodec).
		Body(vz).
		Do(ctx).
		Into(result)
	return result, err
}
