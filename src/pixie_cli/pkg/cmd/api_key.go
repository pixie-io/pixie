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
	"fmt"
	"os"
	"strings"
	"syscall"

	"github.com/gofrs/uuid"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/cobra"
	"github.com/spf13/viper"
	"golang.org/x/term"

	"px.dev/pixie/src/api/proto/cloudpb"
	"px.dev/pixie/src/pixie_cli/pkg/auth"
	"px.dev/pixie/src/pixie_cli/pkg/components"
	"px.dev/pixie/src/pixie_cli/pkg/utils"
	utils2 "px.dev/pixie/src/utils"
)

func init() {
	APIKeyCmd.AddCommand(CreateAPIKeyCmd)
	APIKeyCmd.AddCommand(DeleteAPIKeyCmd)
	APIKeyCmd.AddCommand(ListAPIKeyCmd)
	APIKeyCmd.AddCommand(GetAPIKeyCmd)
	APIKeyCmd.AddCommand(LookupAPIKeyCmd)

	CreateAPIKeyCmd.Flags().StringP("desc", "d", "", "A description for the API key")
	CreateAPIKeyCmd.Flags().BoolP("short", "s", false, "Return only the created API key, for use to pipe to other tools")

	DeleteAPIKeyCmd.Flags().StringP("id", "i", "", "The API key to delete")

	ListAPIKeyCmd.Flags().StringP("output", "o", "", "Output format: one of: json|proto")

	LookupAPIKeyCmd.Flags().StringP("key", "k", "", "Value of the key. Leave blank to be prompted.")
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
	PreRun: func(cmd *cobra.Command, args []string) {
		viper.BindPFlag("desc", cmd.Flags().Lookup("desc"))
		viper.BindPFlag("short", cmd.Flags().Lookup("short"))
	},
	Run: func(cmd *cobra.Command, args []string) {
		cloudAddr := viper.GetString("cloud_addr")
		desc, _ := cmd.Flags().GetString("desc")
		short, _ := cmd.Flags().GetBool("short")

		keyID, key, err := generateAPIKey(cloudAddr, desc)
		if err != nil {
			// Using log.Fatal rather than CLI log in order to track this unexpected error in Sentry.
			log.WithError(err).Fatal("Failed to generate API key")
		}
		if short {
			fmt.Fprintf(os.Stdout, "%s\n", key)
		} else {
			utils.Infof("Generated API key: \nID: %s \nKey: %s", keyID, key)
		}
	},
}

// DeleteAPIKeyCmd is the Delete sub-command of APIKey.
var DeleteAPIKeyCmd = &cobra.Command{
	Use:   "delete",
	Short: "Delete a API key for Pixie",
	PreRun: func(cmd *cobra.Command, args []string) {
		viper.BindPFlag("id", cmd.Flags().Lookup("id"))
	},
	Run: func(cmd *cobra.Command, args []string) {
		cloudAddr := viper.GetString("cloud_addr")
		id, _ := cmd.Flags().GetString("id")
		if id == "" {
			utils.Fatal("API key ID must be specified using --id flag")
		}

		idUUID, err := uuid.FromString(id)
		if err != nil {
			utils.WithError(err).Fatal("Invalid API key ID")
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
	Short: "List all API key metadata",
	PreRun: func(cmd *cobra.Command, args []string) {
		viper.BindPFlag("output", cmd.Flags().Lookup("output"))
	},
	Run: func(cmd *cobra.Command, args []string) {
		cloudAddr := viper.GetString("cloud_addr")
		format, _ := cmd.Flags().GetString("output")
		format = strings.ToLower(format)

		keys, err := listAPIKeyMetadatas(cloudAddr)
		if err != nil {
			// Using log.Fatal rather than CLI log in order to track this unexpected error in Sentry.
			log.WithError(err).Fatal("Failed to list API keys")
		}
		// Throw keys into table.
		w := components.CreateStreamWriter(format, os.Stdout)
		defer w.Finish()
		w.SetHeader("api-keys", []string{"ID", "Key", "CreatedAt", "Description"})
		for _, k := range keys {
			_ = w.Write([]interface{}{utils2.UUIDFromProtoOrNil(k.ID), "<hidden>", k.CreatedAt,
				k.Desc})
		}
	},
}

// LookupAPIKeyCmd is looks up the API key using the actual key value;
var LookupAPIKeyCmd = &cobra.Command{
	Use:   "lookup",
	Short: "Lookup API key based on the value of the key",
	Run: func(cmd *cobra.Command, args []string) {
		cloudAddr := viper.GetString("cloud_addr")
		format, _ := cmd.Flags().GetString("output")
		format = strings.ToLower(format)
		apiKey, err := cmd.Flags().GetString("key")
		if err != nil || len(apiKey) == 0 {
			fmt.Print("\nEnter API Key (won't echo): ")
			k, err := term.ReadPassword(syscall.Stdin)
			if err != nil {
				log.WithError(err).Fatal("Failed to read API Key")
			}
			apiKey = string(k)
		}

		k, err := lookupAPIKey(cloudAddr, apiKey)
		if err != nil {
			// Using log.Fatal rather than CLI log in order to track this unexpected error in Sentry.
			log.WithError(err).Fatal("Failed to list lookup key")
		}
		// Throw keys into table.
		w := components.CreateStreamWriter(format, os.Stdout)
		defer w.Finish()
		w.SetHeader("api-keys", []string{"ID", "Key", "CreatedAt", "Description"})
		_ = w.Write([]interface{}{utils2.UUIDFromProtoOrNil(k.ID), "<hidden>", k.CreatedAt,
			k.Desc})
	},
}

// GetAPIKeyCmd is the List sub-command of APIKey.
var GetAPIKeyCmd = &cobra.Command{
	Use:   "get",
	Short: "Get API key details for a specific key",
	Run: func(cmd *cobra.Command, args []string) {
		cloudAddr := viper.GetString("cloud_addr")
		format, _ := cmd.Flags().GetString("output")
		format = strings.ToLower(format)

		if len(args) != 1 {
			utils.Fatal("Expected a single argument 'key id'.")
		}

		keyID, err := uuid.FromString(args[0])
		if err != nil {
			utils.Fatal("Malformed Key ID. Expected a single argument 'key id'.")
		}
		k, err := getAPIKey(cloudAddr, keyID)
		if err != nil {
			// Using log.Fatal rather than CLI log in order to track this unexpected error in Sentry.
			log.WithError(err).Fatal("Failed to fetch API key")
		}
		// Throw keys into table.
		w := components.CreateStreamWriter(format, os.Stdout)
		defer w.Finish()
		w.SetHeader("api-keys", []string{"ID", "Key", "CreatedAt", "Description"})
		_ = w.Write([]interface{}{utils2.UUIDFromProtoOrNil(k.ID), k.Key, k.CreatedAt,
			k.Desc})
	},
}

func getAPIKeyClientAndContext(cloudAddr string) (cloudpb.APIKeyManagerClient, context.Context, error) {
	// Get grpc connection to cloud.
	cloudConn, err := utils.GetCloudClientConnection(cloudAddr)
	if err != nil {
		// Using log.Fatal rather than CLI log in order to track this unexpected error in Sentry.
		log.Fatalln(err)
	}

	// Get client for apiKeyMgr.
	apiKeyMgr := cloudpb.NewAPIKeyManagerClient(cloudConn)

	ctxWithCreds := auth.CtxWithCreds(context.Background())
	return apiKeyMgr, ctxWithCreds, nil
}

func generateAPIKey(cloudAddr string, desc string) (string, string, error) {
	apiKeyMgr, ctxWithCreds, err := getAPIKeyClientAndContext(cloudAddr)
	if err != nil {
		return "", "", err
	}

	resp, err := apiKeyMgr.Create(ctxWithCreds, &cloudpb.CreateAPIKeyRequest{Desc: desc})
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

func listAPIKeyMetadatas(cloudAddr string) ([]*cloudpb.APIKeyMetadata, error) {
	apiKeyMgr, ctxWithCreds, err := getAPIKeyClientAndContext(cloudAddr)
	if err != nil {
		return nil, err
	}

	resp, err := apiKeyMgr.List(ctxWithCreds, &cloudpb.ListAPIKeyRequest{})
	if err != nil {
		return nil, err
	}

	return resp.Keys, nil
}

func getAPIKey(cloudAddr string, keyID uuid.UUID) (*cloudpb.APIKey, error) {
	apiKeyMgr, ctxWithCreds, err := getAPIKeyClientAndContext(cloudAddr)
	if err != nil {
		return nil, err
	}

	resp, err := apiKeyMgr.Get(ctxWithCreds, &cloudpb.GetAPIKeyRequest{
		ID: utils2.ProtoFromUUID(keyID),
	})
	if err != nil {
		return nil, err
	}

	return resp.Key, nil
}

func lookupAPIKey(cloudAddr string, key string) (*cloudpb.APIKey, error) {
	apiKeyMgr, ctxWithCreds, err := getAPIKeyClientAndContext(cloudAddr)
	if err != nil {
		return nil, err
	}

	resp, err := apiKeyMgr.LookupAPIKey(ctxWithCreds, &cloudpb.LookupAPIKeyRequest{Key: key})
	if err != nil {
		return nil, err
	}

	return resp.Key, nil
}
