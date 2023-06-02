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

package scriptrunner

import (
	"github.com/nats-io/nats.go"
	log "github.com/sirupsen/logrus"
	"k8s.io/client-go/rest"

	"px.dev/pixie/src/utils/shared/k8s"
	"px.dev/pixie/src/vizier/services/metadata/metadatapb"
)

// Names of [Source]s
const (
	CloudSourceName     = "cloud"
	ConfigMapSourceName = "configmaps"
)

// DefaultSources is a list of sources enabled by default
var DefaultSources = []string{CloudSourceName}

// Sources initializes multiple sources based on the set of sourceNames provided.
func Sources(nc *nats.Conn, csClient metadatapb.CronScriptStoreServiceClient, signingKey string, namespace string, sourceNames []string) []Source {
	var sources []Source
	for _, selectedName := range sourceNames {
		switch selectedName {
		case CloudSourceName:
			sources = append(sources, NewCloudSource(nc, csClient, signingKey))
		case ConfigMapSourceName:
			kubeConfig, err := rest.InClusterConfig()
			if err != nil {
				log.WithError(err).Fatal("Unable to get incluster kubeconfig")
			}
			client := k8s.GetClientset(kubeConfig)
			sources = append(sources, NewConfigMapSource(client, namespace))
		default:
			log.Errorf(`Unknown source "%s"`, selectedName)
		}
	}
	return sources
}
