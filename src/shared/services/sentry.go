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

package services

import (
	"context"
	"fmt"
	"os"
	"runtime"
	"time"

	"github.com/getsentry/sentry-go"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"
	v1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/client-go/rest"

	"px.dev/pixie/src/operator/client/versioned"
	version "px.dev/pixie/src/shared/goversion"
	"px.dev/pixie/src/shared/services/sentryhook"
)

func init() {
	pflag.String("sentry_dsn", "", "The sentry DSN. Empty disables sentry")
}

func getSentryVal(envNamespaceStr string) (string, error) {
	// There is a specific config for services running in the cluster.
	kubeConfig, err := rest.InClusterConfig()
	if err != nil {
		return "", err
	}

	vzCrdClient, err := versioned.NewForConfig(kubeConfig)
	if err != nil {
		log.WithError(err).Error("Failed to initialize vizier CRD client")
		return "", err
	}

	ctx := context.Background()
	vzLst, err := vzCrdClient.PxV1alpha1().Viziers(envNamespaceStr).List(ctx, v1.ListOptions{})
	if err != nil {
		log.WithError(err).Error("Failed to list vizier CRDs.")
		return "", err
	}

	if len(vzLst.Items) == 1 {
		return vzLst.Items[0].Status.SentryDSN, nil
	} else if len(vzLst.Items) > 0 {
		return "", fmt.Errorf("spec contains more than 1 vizier item")
	}
	return "", fmt.Errorf("spec contains 0 vizier items")
}

// InitDefaultSentry initialize sentry with default options. The options are set based on values
// from viper.
func InitDefaultSentry() func() {
	dsn := viper.GetString("sentry_dsn")

	return InitSentryWithDSN("", dsn)
}

// InitSentryFromCRD attempts to initialize sentry using the sentry DSN value from the Vizier CRD.
func InitSentryFromCRD(id, crdNamespace string) func() {
	dsn, err := getSentryVal(crdNamespace)
	if err != nil {
		dsn = viper.GetString("sentry_dsn")
		log.WithError(err).Trace("Cannot initialize sentry value from Vizier CRD.")
	}

	return InitSentryWithDSN(id, dsn)
}

// InitSentryWithDSN initializes sentry with the given DSN.
func InitSentryWithDSN(id, dsn string) func() {
	podName := viper.GetString("pod_name")
	executable, _ := os.Executable()

	err := sentry.Init(sentry.ClientOptions{
		Dsn:              dsn,
		AttachStacktrace: true,
		Release:          version.GetVersion().ToString(),
		Environment:      runtime.GOOS,
		MaxBreadcrumbs:   10,
	})
	if err != nil {
		log.WithError(err).Trace("Cannot initialize sentry")
	} else {
		tags := map[string]string{
			"version":    version.GetVersion().ToString(),
			"executable": executable,
			"pod_name":   podName,
		}
		if id != "" {
			tags["ID"] = id
		}
		hook := sentryhook.New([]log.Level{
			log.ErrorLevel, log.PanicLevel, log.FatalLevel,
		}, sentryhook.WithTags(tags))
		log.AddHook(hook)
	}

	return func() {
		sentry.Flush(2 * time.Second)
	}
}
