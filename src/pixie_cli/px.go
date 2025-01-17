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

// This is the Pixie Admin CLI.
// It will be responsible for managing and deploy Pixie on a cluster.

import (
	"fmt"
	"os"
	"runtime"
	"strings"
	"time"

	"github.com/getsentry/sentry-go"
	"github.com/segmentio/analytics-go/v3"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/viper"

	"px.dev/pixie/src/pixie_cli/pkg/cmd"
	"px.dev/pixie/src/pixie_cli/pkg/pxanalytics"
	"px.dev/pixie/src/pixie_cli/pkg/pxconfig"
	"px.dev/pixie/src/pixie_cli/pkg/sentryhook"
	"px.dev/pixie/src/pixie_cli/pkg/utils"
	version "px.dev/pixie/src/shared/goversion"
)

const sentryDSN = "https://48c370af36817aad74449b3adc509d78@o4507357617192960.ingest.us.sentry.io/4508004179771392"

func main() {
	// Disable Sentry in dev mode.
	selectedDSN := sentryDSN
	if version.GetVersion().IsDev() {
		selectedDSN = ""
	}
	err := sentry.Init(sentry.ClientOptions{
		Dsn:              selectedDSN,
		AttachStacktrace: true,
		Release:          version.GetVersion().ToString(),
		Environment:      runtime.GOOS,
		MaxBreadcrumbs:   10,
	})
	if err != nil {
		log.WithError(err).Trace("Cannot initialize sentry")
	} else {
		tags := map[string]string{
			"version":  version.GetVersion().ToString(),
			"clientID": pxconfig.Cfg().UniqueClientID,
		}
		hook := sentryhook.New([]log.Level{
			log.ErrorLevel, log.PanicLevel, log.FatalLevel,
		}, sentryhook.WithTags(tags))
		log.AddHook(hook)
	}
	defer sentry.Flush(2 * time.Second)
	defer pxanalytics.Client().Close()

	pxanalytics.Client().Enqueue(&analytics.Track{
		UserId: pxconfig.Cfg().UniqueClientID,
		Event:  "Exec Started",
		Properties: analytics.NewProperties().
			Set("cmd", strings.Join(os.Args, ",")),
	})

	defer pxanalytics.Client().Enqueue(&analytics.Track{
		UserId: pxconfig.Cfg().UniqueClientID,
		Event:  "Exec Complete",
	})

	utils.Info("Pixie CLI")

	logFile := viper.GetString("log_file")
	if len(logFile) > 0 {
		utils.Info(fmt.Sprintf("Logging to %s", logFile))

		f, err := os.OpenFile(logFile, os.O_WRONLY|os.O_CREATE|os.O_APPEND, 0644)
		if err != nil {
			log.WithError(err).Error("Cannot open log file")
		}

		defer f.Close()
		log.SetOutput(f)
	} else {
		log.SetOutput(os.Stderr)
	}
	cmd.Execute()
}
