package cmd

import (
	"context"
	"os"
	"strings"

	"github.com/gofrs/uuid"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/cobra"
	"github.com/spf13/viper"

	"pixielabs.ai/pixielabs/src/cloud/cloudapipb"
	"pixielabs.ai/pixielabs/src/pixie_cli/pkg/auth"
	"pixielabs.ai/pixielabs/src/pixie_cli/pkg/components"
	"pixielabs.ai/pixielabs/src/pixie_cli/pkg/utils"
	utils2 "pixielabs.ai/pixielabs/src/utils"
)

func init() {
	DeployKeyCmd.AddCommand(CreateDeployKeyCmd)
	DeployKeyCmd.AddCommand(DeleteDeployKeyCmd)
	DeployKeyCmd.AddCommand(ListDeployKeyCmd)

	CreateDeployKeyCmd.Flags().StringP("desc", "d", "", "A description for the deploy key")
	viper.BindPFlag("desc", CreateDeployKeyCmd.Flags().Lookup("desc"))

	DeleteDeployKeyCmd.Flags().StringP("id", "i", "", "The deploy key to delete")
	viper.BindPFlag("id", DeleteDeployKeyCmd.Flags().Lookup("id"))

	ListDeployKeyCmd.Flags().StringP("output", "o", "", "Output format: one of: json|proto")
	viper.BindPFlag("output", ListDeployKeyCmd.Flags().Lookup("output"))
}

// DeployKeyCmd is the deploy-key sub-command of the CLI.
var DeployKeyCmd = &cobra.Command{
	Use:   "deploy-key",
	Short: "Manage deployment keys for Pixie",
	Run: func(cmd *cobra.Command, args []string) {
		utils.Info("Nothing here... Please execute one of the subcommands")
		cmd.Help()
		return
	},
}

// CreateDeployKeyCmd is the Create sub-command of DeployKey.
var CreateDeployKeyCmd = &cobra.Command{
	Use:   "create",
	Short: "Generate a deploy key for Pixie",
	Run: func(cmd *cobra.Command, args []string) {
		cloudAddr := viper.GetString("cloud_addr")
		desc := viper.GetString("desc")

		keyID, key, err := generateDeployKey(cloudAddr, desc)
		if err != nil {
			// Using log.Fatal rather than CLI log in order to track this unexpected error in Sentry.
			log.WithError(err).Fatal("Failed to generate deployment key")
		}
		utils.Infof("Generated deployment key: \nID: %s \nKey: %s", keyID, key)
	},
}

// DeleteDeployKeyCmd is the Delete sub-command of DeployKey.
var DeleteDeployKeyCmd = &cobra.Command{
	Use:   "delete",
	Short: "Delete a deploy key for Pixie",
	Run: func(cmd *cobra.Command, args []string) {
		cloudAddr := viper.GetString("cloud_addr")
		id := viper.GetString("id")
		if id == "" {
			utils.Error("Deployment key ID must be specified using --id flag")
			os.Exit(1)
		}

		idUUID, err := uuid.FromString(id)
		if err != nil {
			utils.WithError(err).Error("Invalid deployment key ID")
			os.Exit(1)
		}

		err = deleteDeployKey(cloudAddr, idUUID)
		if err != nil {
			// Using log.Fatal rather than CLI log in order to track this unexpected error in Sentry.
			log.WithError(err).Fatal("Failed to delete deployment key")
		}
		utils.Info("Successfully deleted deployment key")
	},
}

// ListDeployKeyCmd is the List sub-command of DeployKey.
var ListDeployKeyCmd = &cobra.Command{
	Use:   "list",
	Short: "List all deploy key for Pixie",
	Run: func(cmd *cobra.Command, args []string) {
		cloudAddr := viper.GetString("cloud_addr")
		format, _ := cmd.Flags().GetString("output")
		format = strings.ToLower(format)

		keys, err := listDeployKeys(cloudAddr)
		if err != nil {
			// Using log.Fatal rather than CLI log in order to track this unexpected error in Sentry.
			log.WithError(err).Fatal("Failed to list deployment keys")
		}
		// Throw keys into table.
		w := components.CreateStreamWriter(format, os.Stdout)
		defer w.Finish()
		w.SetHeader("deployment-keys", []string{"ID", "Key", "CreatedAt", "Description"})
		for _, k := range keys {
			_ = w.Write([]interface{}{utils2.UUIDFromProtoOrNil(k.ID), k.Key, k.CreatedAt,
				k.Desc})
		}
	},
}

func getClientAndContext(cloudAddr string) (cloudapipb.VizierDeploymentKeyManagerClient, context.Context, error) {
	// Get grpc connection to cloud.
	cloudConn, err := utils.GetCloudClientConnection(cloudAddr)
	if err != nil {
		// Using log.Fatal rather than CLI log in order to track this unexpected error in Sentry.
		log.Fatalln(err)
	}

	// Get client for deployKeyMgr.
	deployMgrClient := cloudapipb.NewVizierDeploymentKeyManagerClient(cloudConn)

	ctxWithCreds := auth.CtxWithCreds(context.Background())
	return deployMgrClient, ctxWithCreds, nil
}

func generateDeployKey(cloudAddr string, desc string) (string, string, error) {
	deployMgrClient, ctxWithCreds, err := getClientAndContext(cloudAddr)
	if err != nil {
		return "", "", err
	}

	resp, err := deployMgrClient.Create(ctxWithCreds, &cloudapipb.CreateDeploymentKeyRequest{Desc: desc})
	if err != nil {
		return "", "", err
	}

	return utils2.UUIDFromProtoOrNil(resp.ID).String(), resp.Key, nil
}

func deleteDeployKey(cloudAddr string, keyID uuid.UUID) error {
	deployMgrClient, ctxWithCreds, err := getClientAndContext(cloudAddr)
	if err != nil {
		return err
	}

	_, err = deployMgrClient.Delete(ctxWithCreds, utils2.ProtoFromUUID(keyID))
	return err
}

func listDeployKeys(cloudAddr string) ([]*cloudapipb.DeploymentKey, error) {
	deployMgrClient, ctxWithCreds, err := getClientAndContext(cloudAddr)
	if err != nil {
		return nil, err
	}

	resp, err := deployMgrClient.List(ctxWithCreds, &cloudapipb.ListDeploymentKeyRequest{})
	if err != nil {
		return nil, err
	}

	return resp.Keys, nil
}
