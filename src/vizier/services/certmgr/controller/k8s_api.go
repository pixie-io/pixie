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

package controller

import (
	"context"
	"crypto/tls"
	"fmt"

	log "github.com/sirupsen/logrus"
	v1 "k8s.io/api/core/v1"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/apimachinery/pkg/labels"
	"k8s.io/client-go/kubernetes"
	"k8s.io/client-go/rest"

	// Blank import necessary for kubeConfig to work.
	_ "k8s.io/client-go/plugin/pkg/client/auth/gcp"
)

// K8sAPIImpl is a wrapper around the k8s API.
type K8sAPIImpl struct {
	clientset *kubernetes.Clientset
	namespace string
}

// NewK8sAPI creates a new K8sAPIImpl.
func NewK8sAPI(namespace string) (*K8sAPIImpl, error) {
	// There is a specific config for services running in the cluster.
	kubeConfig, err := rest.InClusterConfig()
	if err != nil {
		return nil, err
	}
	log.Info("Got cluster config")

	// Create k8s client.
	clientset, err := kubernetes.NewForConfig(kubeConfig)
	if err != nil {
		return nil, err
	}
	log.Info("Created k8s client")

	k8sAPI := &K8sAPIImpl{clientset: clientset, namespace: namespace}
	return k8sAPI, nil
}

// CreateTLSSecret creates a new TLS secret with the given key and cert.
func (k *K8sAPIImpl) CreateTLSSecret(name string, key string, cert string) error {
	// Delete secret before creating.
	err := k.clientset.CoreV1().Secrets(k.namespace).Delete(context.Background(), name, metav1.DeleteOptions{})
	if err != nil {
		log.WithError(err).Debug("could not delete secret")
	} else {
		log.Info(fmt.Sprintf("Deleted secret: %s", name))
	}

	if _, err := tls.X509KeyPair([]byte(cert), []byte(key)); err != nil {
		return err
	}

	secret := &v1.Secret{}
	secret.Name = name
	secret.Type = v1.SecretTypeTLS
	secret.Data = map[string][]byte{}
	secret.Data[v1.TLSCertKey] = []byte(cert)
	secret.Data[v1.TLSPrivateKeyKey] = []byte(key)

	_, err = k.clientset.CoreV1().Secrets(k.namespace).Create(context.Background(), secret, metav1.CreateOptions{})
	if err != nil {
		return err
	}

	log.Info(fmt.Sprintf("Created TLS secret: %s", name))
	return nil
}

// GetPodNamesForService gets the pod names for the given service.
func (k *K8sAPIImpl) GetPodNamesForService(name string) ([]string, error) {
	svc, err := k.clientset.CoreV1().Services(k.namespace).Get(context.Background(), name, metav1.GetOptions{})
	if err != nil {
		return []string{}, err
	}

	set := labels.Set(svc.Spec.Selector)

	pods, err := k.clientset.CoreV1().Pods(k.namespace).List(context.Background(), metav1.ListOptions{LabelSelector: set.String()})
	if err != nil {
		return []string{}, err
	}

	podNames := make([]string, len(pods.Items))
	for idx, v := range pods.Items {
		podNames[idx] = v.GetName()
	}

	return podNames, nil
}

// DeletePod deletes the pod with the given name.
func (k *K8sAPIImpl) DeletePod(name string) error {
	err := k.clientset.CoreV1().Pods(k.namespace).Delete(context.Background(), name, metav1.DeleteOptions{})
	return err
}
