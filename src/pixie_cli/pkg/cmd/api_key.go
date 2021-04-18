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
	"context"
	"os"
	"strings"

	"github.com/gofrs/uuid"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/cobra"
	"github.com/spf13/viper"

	"px.dev/pixie/src/cloud/cloudapipb"
	"px.dev/pixie/src/pixie_cli/pkg/auth"
	"px.dev/pixie/src/pixie_cli/pkg/components"
	"px.dev/pixie/src/pixie_cli/pkg/utils"
	utils2 "px.dev/pixie/src/utils"
)

func init() {
	APIKeyCmd.AddCommand(CreateAPIKeyCmd)
	APIKeyCmd.AddCommand(DeleteAPIKeyCmd)
	APIKeyCmd.AddCommand(ListAPIKeyCmd)

	CreateAPIKeyCmd.Flags().StringP("desc", "d", "", "A description for the API key")
	viper.BindPFlag("desc", CreateAPIKeyCmd.Flags().Lookup("desc"))

	DeleteAPIKeyCmd.Flags().StringP("id", "i", "", "The API key to delete")
	viper.BindPFlag("id", DeleteAPIKeyCmd.Flags().Lookup("id"))

	ListAPIKeyCmd.Flags().StringP("output", "o", "", "Output format: one of: json|proto")
	viper.BindPFlag("output", ListAPIKeyCmd.Flags().Lookup("output"))
}

// APIKeyCmd is the api-key sub-command of the CLI.
var APIKeyCmd = &cobra.Command{
	Use:   "api-key",
	Short: "Manage API keys for Pixie",
	Run: func(cmd *cobra.Command, args []string) {
		utils.Info("Nothing here... Please execute one of the subcommands")
		cmd.Help()
	},
}

// CreateAPIKeyCmd is the Create sub-command of APIKey.
var CreateAPIKeyCmd = &cobra.Command{
	Use:   "create",
	Short: "Generate a API key for Pixie",
	Run: func(cmd *cobra.Command, args []string) {
		cloudAddr := viper.GetString("cloud_addr")
		desc, _ := cmd.Flags().GetString("desc")

		keyID, key, err := generateAPIKey(cloudAddr, desc)
		if err != nil {
			// Using log.Fatal rather than CLI log in order to track this unexpected error in Sentry.
			log.WithError(err).Fatal("Failed to generate API key")
		}
		utils.Infof("Generated API key: \nID: %s \nKey: %s", keyID, key)
	},
}

// DeleteAPIKeyCmd is the Delete sub-command of APIKey.
var DeleteAPIKeyCmd = &cobra.Command{
	Use:   "delete",
	Short: "Delete a API key for Pixie",
	Run: func(cmd *cobra.Command, args []string) {
		cloudAddr := viper.GetString("cloud_addr")
		id, _ := cmd.Flags().GetString("id")
		if id == "" {
			utils.Error("API key ID must be specified using --id flag")
			os.Exit(1)
		}

		idUUID, err := uuid.FromString(id)
		if err != nil {
			utils.WithError(err).Error("Invalid API key ID")
			os.Exit(1)
		}

		err = deleteAPIKey(cloudAddr, idUUID)
		if err != nil {
			// Using log.Fatal rather than CLI log in order to track this unexpected error in Sentry.
			log.WithError(err).Fatal("Failed to delete API key")
		}
		utils.Info("Successfully deleted API key")
	},
}

// ListAPIKeyCmd is the List sub-command of APIKey.
var ListAPIKeyCmd = &cobra.Command{
	Use:   "list",
	Short: "List all API key for Pixie",
	Run: func(cmd *cobra.Command, args []string) {
		cloudAddr := viper.GetString("cloud_addr")
		format, _ := cmd.Flags().GetString("output")
		format = strings.ToLower(format)

		keys, err := listAPIKeys(cloudAddr)
		if err != nil {
			// Using log.Fatal rather than CLI log in order to track this unexpected error in Sentry.
			log.WithError(err).Fatal("Failed to list API keys")
		}
		// Throw keys into table.
		w := components.CreateStreamWriter(format, os.Stdout)
		defer w.Finish()
		w.SetHeader("api-keys", []string{"ID", "Key", "CreatedAt", "Description"})
		for _, k := range keys {
			_ = w.Write([]interface{}{utils2.UUIDFromProtoOrNil(k.ID), k.Key, k.CreatedAt,
				k.Desc})
		}
	},
}

func getAPIKeyClientAndContext(cloudAddr string) (cloudapipb.APIKeyManagerClient, context.Context, error) {
	// Get grpc connection to cloud.
	cloudConn, err := utils.GetCloudClientConnection(cloudAddr)
	if err != nil {
		// Using log.Fatal rather than CLI log in order to track this unexpected error in Sentry.
		log.Fatalln(err)
	}

	// Get client for apiKeyMgr.
	apiKeyMgr := cloudapipb.NewAPIKeyManagerClient(cloudConn)

	ctxWithCreds := auth.CtxWithCreds(context.Background())
	return apiKeyMgr, ctxWithCreds, nil
}

func generateAPIKey(cloudAddr string, desc string) (string, string, error) {
	apiKeyMgr, ctxWithCreds, err := getAPIKeyClientAndContext(cloudAddr)
	if err != nil {
		return "", "", err
	}

	resp, err := apiKeyMgr.Create(ctxWithCreds, &cloudapipb.CreateAPIKeyRequest{Desc: desc})
	if err != nil {
		return "", "", err
	}

	return utils2.UUIDFromProtoOrNil(resp.ID).String(), resp.Key, nil
}

func deleteAPIKey(cloudAddr string, keyID uuid.UUID) error {
	apiKeyMgr, ctxWithCreds, err := getAPIKeyClientAndContext(cloudAddr)
	if err != nil {
		return err
	}

	_, err = apiKeyMgr.Delete(ctxWithCreds, utils2.ProtoFromUUID(keyID))
	return err
}

func listAPIKeys(cloudAddr string) ([]*cloudapipb.APIKey, error) {
	apiKeyMgr, ctxWithCreds, err := getAPIKeyClientAndContext(cloudAddr)
	if err != nil {
		return nil, err
	}

	resp, err := apiKeyMgr.List(ctxWithCreds, &cloudapipb.ListAPIKeyRequest{})
	if err != nil {
		return nil, err
	}

	return resp.Keys, nil
}
