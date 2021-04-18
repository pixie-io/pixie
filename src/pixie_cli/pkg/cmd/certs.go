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
	"github.com/spf13/cobra"
	"github.com/spf13/viper"

	"px.dev/pixie/src/utils/shared/certs"
)

// InstallCertsCmd is the "install-certs" command.
var InstallCertsCmd = &cobra.Command{
	Use:   "install-certs",
	Short: "Generates the proper server and client certs",
	Run: func(cmd *cobra.Command, args []string) {
		certPath, _ := cmd.Flags().GetString("cert_path")
		caCert, _ := cmd.Flags().GetString("ca_cert")
		caKey, _ := cmd.Flags().GetString("ca_key")
		namespace, _ := cmd.Flags().GetString("namespace")
		bitsize, _ := cmd.Flags().GetInt("bit_size")
		certs.InstallCerts(certPath, caCert, caKey, namespace, bitsize)
	},
}

func init() {
	InstallCertsCmd.Flags().StringP("cert_path", "p", "", "Directory to save certs in")
	viper.BindPFlag("cert_path", InstallCertsCmd.Flags().Lookup("cert_path"))
	InstallCertsCmd.Flags().StringP("ca_cert", "c", "", "Path to CA cert (optional)")
	viper.BindPFlag("ca_cert", InstallCertsCmd.Flags().Lookup("ca_cert"))
	InstallCertsCmd.Flags().StringP("ca_key", "k", "", "Path to CA key (optional)")
	viper.BindPFlag("ca_key", InstallCertsCmd.Flags().Lookup("ca_key"))
	InstallCertsCmd.Flags().IntP("bit_size", "b", 4096, "Size in bits of the generated key")
	viper.BindPFlag("bit_size", InstallCertsCmd.Flags().Lookup("bit_size"))
	InstallCertsCmd.Flags().StringP("namespace", "n", "pl", "The namespace to install certs to")
	viper.BindPFlag("namespace", InstallCertsCmd.Flags().Lookup("namespace"))
}
