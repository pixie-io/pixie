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
	"bytes"
	"context"
	"crypto/tls"
	"encoding/base64"
	"encoding/json"
	"fmt"
	"os"
	"strings"

	log "github.com/sirupsen/logrus"

	v1 "k8s.io/api/core/v1"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/apimachinery/pkg/util/validation"
	"k8s.io/client-go/kubernetes"
)

// DeleteSecret deletes the given secret in kubernetes.
func DeleteSecret(clientset kubernetes.Interface, namespace, name string) {
	err := clientset.CoreV1().Secrets(namespace).Delete(context.Background(), name, metav1.DeleteOptions{})
	if err != nil {
		log.WithError(err).Error("Failed to delete secret")
	}
}

// GetSecret gets the secret in kubernetes.
func GetSecret(clientset kubernetes.Interface, namespace, name string) *v1.Secret {
	secret, err := clientset.CoreV1().Secrets(namespace).Get(context.Background(), name, metav1.GetOptions{})
	if err != nil {
		return nil
	}

	return secret
}

// Contents below are copied and modified from
// https://github.com/kubernetes/kubectl/blob/3874cf79897cfe1e070e592391792658c44b78d4/pkg/generate/versioned/secret.go.

// CreateGenericSecret creates a generic secret in kubernetes.
func CreateGenericSecret(namespace, name string, fromFiles map[string]string) (*v1.Secret, error) {
	secret := &v1.Secret{}
	secret.SetGroupVersionKind(v1.SchemeGroupVersion.WithKind("Secret"))

	secret.Name = name
	secret.Data = map[string][]byte{}
	secret.Namespace = namespace

	err := handleFromFileSources(secret, fromFiles)
	if err != nil {
		return nil, err
	}

	return secret, nil
}

// CreateGenericSecretFromLiterals creates a generic secret in kubernetes using literals.
func CreateGenericSecretFromLiterals(namespace, name string, fromLiterals map[string]string) (*v1.Secret, error) {
	secret := &v1.Secret{}
	secret.SetGroupVersionKind(v1.SchemeGroupVersion.WithKind("Secret"))

	secret.Name = name
	secret.Data = map[string][]byte{}
	secret.Namespace = namespace

	for k, v := range fromLiterals {
		secret.Data[k] = []byte(v)
	}

	return secret, nil
}

// CreateConfigMapFromLiterals creates a configmap in kubernetes using literals.
func CreateConfigMapFromLiterals(namespace, name string, fromLiterals map[string]string) (*v1.ConfigMap, error) {
	cm := &v1.ConfigMap{}
	cm.SetGroupVersionKind(v1.SchemeGroupVersion.WithKind("ConfigMap"))

	cm.Name = name
	cm.Data = map[string]string{}
	cm.Namespace = namespace

	for k, v := range fromLiterals {
		cm.Data[k] = v
	}

	return cm, nil
}

// CreateTLSSecret creates a TLS secret in kubernetes.
func CreateTLSSecret(namespace, name string, key string, cert string) (*v1.Secret, error) {
	tlsCrt, err := readFile(cert)
	if err != nil {
		return nil, err
	}
	tlsKey, err := readFile(key)
	if err != nil {
		return nil, err
	}

	if _, err := tls.X509KeyPair(tlsCrt, tlsKey); err != nil {
		return nil, err
	}

	secret := &v1.Secret{}
	secret.SetGroupVersionKind(v1.SchemeGroupVersion.WithKind("Secret"))
	secret.Name = name
	secret.Type = v1.SecretTypeTLS
	secret.Data = map[string][]byte{}
	secret.Data[v1.TLSCertKey] = []byte(tlsCrt)
	secret.Data[v1.TLSPrivateKeyKey] = []byte(tlsKey)
	secret.Namespace = namespace

	return secret, nil
}

// CreateDockerConfigJSONSecret creates a secret in the docker config format.
// Currently the golang v1.Secret API doesn't perform the massaging of the credentials file that invoking
// kubectl with a docker-registry secret (like below) does.
func CreateDockerConfigJSONSecret(namespace, name, credsData string) (*v1.Secret, error) {
	secret := &v1.Secret{}
	secret.SetGroupVersionKind(v1.SchemeGroupVersion.WithKind("Secret"))
	secret.Name = name
	secret.Type = v1.SecretTypeDockerConfigJson
	secret.Data = map[string][]byte{}

	type dockerSecret struct {
		Username string `json:"username"`
		Password string `json:"password"`
		Auth     string `json:"auth"`
	}

	type dockerConfigAuth struct {
		GcrIO dockerSecret `json:"gcr.io"`
	}

	type dockerConfig struct {
		Auths dockerConfigAuth `json:"auths"`
	}

	encodedCredsData := "_json_key:" + credsData

	credsBytes := []byte(encodedCredsData)
	encodedCreds := new(bytes.Buffer)
	encoder := base64.NewEncoder(base64.StdEncoding, encodedCreds)
	_, err := encoder.Write(credsBytes)
	if err != nil {
		return nil, err
	}
	encoder.Close()

	s := dockerConfig{
		Auths: dockerConfigAuth{
			GcrIO: dockerSecret{
				Username: "_json_key",
				Password: credsData,
				Auth:     encodedCreds.String(),
			},
		},
	}

	secretJSON, err := json.Marshal(s)
	if err != nil {
		return nil, err
	}

	secret.Data[v1.DockerConfigJsonKey] = secretJSON
	secret.Namespace = namespace

	return secret, nil
}

// readFile just reads a file into a byte array.
func readFile(file string) ([]byte, error) {
	b, err := os.ReadFile(file)
	if err != nil {
		return []byte{}, fmt.Errorf("Cannot read file %v, %v", file, err)
	}
	return b, nil
}

func handleFromFileSources(secret *v1.Secret, fromFiles map[string]string) error {
	for keyName, filePath := range fromFiles {
		err := addKeyFromFileToSecret(secret, keyName, filePath)
		if err != nil {
			return err
		}
	}
	return nil
}

func addKeyFromFileToSecret(secret *v1.Secret, keyName, filePath string) error {
	data, err := os.ReadFile(filePath)
	if err != nil {
		return err
	}
	return addKeyFromLiteralToSecret(secret, keyName, data)
}

func addKeyFromLiteralToSecret(secret *v1.Secret, keyName string, data []byte) error {
	if errs := validation.IsConfigMapKey(keyName); len(errs) != 0 {
		return fmt.Errorf("%q is not a valid key name for a Secret: %s", keyName, strings.Join(errs, ";"))
	}

	if _, entryExists := secret.Data[keyName]; entryExists {
		return fmt.Errorf("cannot add key %s, another key by that name already exists", keyName)
	}
	secret.Data[keyName] = data
	return nil
}
