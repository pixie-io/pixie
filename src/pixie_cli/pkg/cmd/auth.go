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
	"github.com/dgrijalva/jwt-go/v4"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/cobra"
	"github.com/spf13/viper"
	"gopkg.in/segmentio/analytics-go.v3"

	"px.dev/pixie/src/pixie_cli/pkg/auth"
	"px.dev/pixie/src/pixie_cli/pkg/components"
	"px.dev/pixie/src/pixie_cli/pkg/pxanalytics"
	"px.dev/pixie/src/pixie_cli/pkg/pxconfig"
	"px.dev/pixie/src/pixie_cli/pkg/utils"
)

func init() {
	AuthCmd.AddCommand(LoginCmd)

	AuthCmd.PersistentFlags().Bool("manual", false, "Don't automatically open the browser")
	viper.BindPFlag("manual", AuthCmd.PersistentFlags().Lookup("manual"))

	AuthCmd.PersistentFlags().String("org_name", "", "Select ORG to login into")
	viper.BindPFlag("org_name", AuthCmd.PersistentFlags().Lookup("org_name"))
	AuthCmd.PersistentFlags().MarkHidden("org_name")
}

// AuthCmd is the auth sub-command of the CLI.
var AuthCmd = &cobra.Command{
	Use:   "auth",
	Short: "Authenticate with Pixie",
	Run: func(cmd *cobra.Command, args []string) {
		utils.Info("Nothing here... Please execute one of the subcommands")
		cmd.Help()
	},
}

// LoginCmd is the Login sub-command of Auth.
var LoginCmd = &cobra.Command{
	Use:   "login",
	Short: "Login to Pixie",
	Run: func(cmd *cobra.Command, args []string) {
		orgName := viper.GetString("org_name")
		l := auth.PixieCloudLogin{
			ManualMode: viper.GetBool("manual"),
			CloudAddr:  viper.GetString("cloud_addr"),
			OrgName:    orgName,
		}
		var refreshToken *auth.RefreshToken
		var err error
		if refreshToken, err = l.Run(); err != nil {
			// Using log.Fatal rather than CLI log in order to track this unexpected error in Sentry.
			log.WithError(err).Fatal("Failed to login")
		}
		if err = auth.SaveRefreshToken(refreshToken); err != nil {
			// Using log.Fatal rather than CLI log in order to track this unexpected error in Sentry.
			log.WithError(err).Fatal("Failed to persist auth token")
		}

		if token, _ := jwt.Parse(refreshToken.Token, nil); token != nil {
			sc, ok := token.Claims.(jwt.MapClaims)
			if ok {
				userID, _ := sc["UserID"].(string)
				// Associate UserID with AnalyticsID.
				_ = pxanalytics.Client().Enqueue(&analytics.Alias{
					UserId:     pxconfig.Cfg().UniqueClientID,
					PreviousId: userID,
				})
			}
		}
		utils.Info("Authentication Successful")

		if orgName != "" {
			components.RenderBureaucratDragon(orgName)
		}
	},
}
