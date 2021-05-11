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

package cmd

import (
	"os"
	"os/signal"

	log "github.com/sirupsen/logrus"
	"github.com/spf13/cobra"
	"github.com/spf13/viper"

	"px.dev/pixie/src/pixie_cli/pkg/utils"
	"px.dev/pixie/src/pixie_cli/pkg/vizier"
	"px.dev/pixie/src/utils/shared/k8s"
)

func init() {
	ProxyCmd.Flags().StringP("namespace", "n", "", "The namespace to install K8s secrets to")
	viper.BindPFlag("namespace", ProxyCmd.Flags().Lookup("namespace"))
}

// ProxyCmd is the "proxy" command.
var ProxyCmd = &cobra.Command{
	Use:   "proxy",
	Short: "Create a proxy connection to Pixie's vizier",
	Run: func(cmd *cobra.Command, args []string) {
		ns, _ := cmd.Flags().GetString("namespace")
		if ns == "" {
			ns = vizier.MustFindVizierNamespace()
		}

		p := k8s.NewVizierProxy(ns)
		if err := p.Run(); err != nil {
			// Using log.Fatal rather than CLI log in order to track this unexpected error in Sentry.
			log.WithError(err).Fatal("Failed to start proxy")
		}

		stop := make(chan os.Signal, 1)
		signal.Notify(stop, os.Interrupt)

		// Wait for interrupt.
		<-stop
		utils.Info("Stopping proxy")
		_ = p.Stop()
	},
}
