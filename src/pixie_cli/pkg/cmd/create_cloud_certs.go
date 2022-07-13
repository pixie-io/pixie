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
	"fmt"
	"os"

	"github.com/spf13/cobra"
	"github.com/spf13/viper"

	"px.dev/pixie/src/utils/shared/certs"
)

func init() {
	CreateCloudCertsCmd.Flags().StringP("namespace", "n", "plc", "The namespace for the cloud services.")
}

// CreateCloudCertsCmd is the "create-cloud-certs" command.
var CreateCloudCertsCmd = &cobra.Command{
	Use:    "create-cloud-certs",
	Short:  "Creates a yaml with server and client certs",
	Hidden: true,
	PreRun: func(cmd *cobra.Command, args []string) {
		viper.BindPFlag("namespace", cmd.Flags().Lookup("namespace"))
	},
	Run: func(cmd *cobra.Command, args []string) {
		namespace, _ := cmd.Flags().GetString("namespace")
		yaml, err := certs.GenerateCloudCertYAMLs(namespace)
		if err != nil {
			fmt.Printf("error while creating service tls certs: %v", err)
			os.Exit(1)
		}
		fmt.Print(yaml)
	},
}
