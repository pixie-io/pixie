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

package main

import (
	"context"
	"crypto/rand"
	"fmt"
	"strings"

	log "github.com/sirupsen/logrus"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/client-go/kubernetes"
	"k8s.io/client-go/rest"

	"px.dev/pixie/src/utils/shared/certs"
	"px.dev/pixie/src/utils/shared/k8s"
	yamlsutils "px.dev/pixie/src/utils/shared/yamls"
	vizieryamls "px.dev/pixie/src/utils/template_generator/vizier_yamls"
)

func init() {
	pflag.String("namespace", "pl", "The namespace used by Pixie")
	pflag.String("custom_labels", "", "Custom labels that should be attached to the vizier resources")
	pflag.String("custom_annotations", "", "Custom annotations that should be attached to the vizier resources")
}

const clusterSecretJWTKey = "jwt-signing-key"

func main() {
	pflag.Parse()

	// Must call after all flags are setup.
	viper.AutomaticEnv()
	viper.SetEnvPrefix("PL")
	viper.BindPFlags(pflag.CommandLine)

	kubeConfig, err := rest.InClusterConfig()
	if err != nil {
		log.WithError(err).Fatal("Failed to create in cluster config")
	}
	// Create k8s client.
	clientset, err := kubernetes.NewForConfig(kubeConfig)
	if err != nil {
		log.WithError(err).Fatal("Failed to create in cluster client set")
	}

	log.Info("Checking if certs already exist...")

	ns := viper.GetString("namespace")
	s := k8s.GetSecret(clientset, ns, "proxy-tls-certs")
	jwtSecret := k8s.GetSecret(clientset, ns, "pl-cluster-secrets")

	if s != nil && jwtSecret != nil && jwtSecret.Data[clusterSecretJWTKey] != nil {
		log.Info("Certs already exist... Exiting job")
		return
	}

	// Assign JWT signing key.
	jwtSigningKey := make([]byte, 64)
	_, err = rand.Read(jwtSigningKey)
	if err != nil {
		log.WithError(err).Fatal("Could not generate JWT signing key")
	}
	s = k8s.GetSecret(clientset, ns, "pl-cluster-secrets")
	if s == nil {
		log.Fatal("pl-cluster-secrets does not exist")
	}
	s.Data[clusterSecretJWTKey] = []byte(fmt.Sprintf("%x", jwtSigningKey))

	_, err = clientset.CoreV1().Secrets(ns).Update(context.Background(), s, metav1.UpdateOptions{})
	if err != nil {
		log.WithError(err).Fatal("Could not update cluster secrets")
	}

	certYAMLs, err := certs.GenerateVizierCertYAMLs(ns)
	if err != nil {
		log.WithError(err).Fatal("Failed to generate cert YAMLs")
	}

	templatedYAML, err := yamlsutils.TemplatizeK8sYAML(certYAMLs, vizieryamls.GlobalTemplateOptions)
	if err != nil {
		log.WithError(err).Fatal("Failed to templatize cert YAMLs")
	}
	tmplValues := &vizieryamls.VizierTmplValues{
		CustomAnnotations: viper.GetString("custom_annotations"),
		CustomLabels:      viper.GetString("custom_labels"),
		Namespace:         ns,
	}
	yamls, err := yamlsutils.ExecuteTemplatedYAMLs([]*yamlsutils.YAMLFile{
		{Name: "certs", YAML: templatedYAML},
	}, vizieryamls.VizierTmplValuesToArgs(tmplValues))
	if err != nil {
		log.WithError(err).Fatal("Failed to fill in templated deployment YAMLs")
	}

	err = k8s.ApplyYAML(clientset, kubeConfig, ns, strings.NewReader(yamls[0].YAML), false)
	if err != nil {
		log.WithError(err).Fatalf("Failed deploy cert YAMLs")
	}

	log.Info("Done provisioning certs!")
}
