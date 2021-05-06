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
	log "github.com/sirupsen/logrus"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"

	"px.dev/pixie/src/utils/shared/k8s"
	"px.dev/pixie/src/utils/shared/yamls"
	vizieryamls "px.dev/pixie/src/utils/template_generator/vizier_yamls"
)

func init() {
	pflag.String("base", "", "Path to the tar containing the base Vizier YAMLs")
	pflag.String("out", "", "The output path")
	pflag.String("version", "", "The version string for the YAMLs")
	pflag.String("namespace", "pl", "The namespace to install K8s secrets to")
}

func main() {
	pflag.Parse()

	viper.AutomaticEnv()
	viper.SetEnvPrefix("PL")
	viper.BindPFlags(pflag.CommandLine)

	base := viper.GetString("base")
	out := viper.GetString("out")
	version := viper.GetString("version")
	namespace := viper.GetString("namespace")

	if len(base) == 0 {
		log.Fatalln("Base YAML path (--base) is required")
	}
	if len(out) == 0 {
		log.Fatalln("Output YAML path (--out) is required")
	}
	if len(version) == 0 {
		log.Fatalln("Version (--version) is required")
	}

	log.WithField("base", base).Info("Base YAML path")
	log.WithField("out", out).Info("Output path")
	log.WithField("version", version).Info("Version")

	kubeConfig := k8s.GetConfig()
	clientset := k8s.GetClientset(kubeConfig)

	templatedYAMLs, err := vizieryamls.GenerateTemplatedDeployYAMLsWithTar(clientset, base, version, namespace)
	if err != nil {
		log.WithError(err).Fatal("failed to generate templated deployment YAMLs")
	}

	if err := yamls.ExtractYAMLs(templatedYAMLs, out, "pixie_yamls", yamls.MultiFileExtractYAMLFormat); err != nil {
		log.WithError(err).Fatal("failed to extract deployment YAMLs")
	}
}
