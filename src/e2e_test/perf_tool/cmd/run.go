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
	"bytes"
	"context"
	"errors"
	"os"
	"os/exec"
	"strings"
	"sync"

	"cloud.google.com/go/bigquery"
	"github.com/gofrs/uuid"
	"github.com/gogo/protobuf/proto"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/cobra"
	"github.com/spf13/viper"

	"px.dev/pixie/src/e2e_test/perf_tool/experimentpb"
	"px.dev/pixie/src/e2e_test/perf_tool/pkg/bq"
	"px.dev/pixie/src/e2e_test/perf_tool/pkg/cluster"
	"px.dev/pixie/src/e2e_test/perf_tool/pkg/cluster/gke"
	"px.dev/pixie/src/e2e_test/perf_tool/pkg/pixie"
	"px.dev/pixie/src/e2e_test/perf_tool/pkg/run"
)

// RunCmd launches a perf experiment by sending queueing the experiment for the px-perf cloud to handle.
var RunCmd = &cobra.Command{
	Use:   "run",
	Short: "Launch perf experiment, managed by the px-perf cluster",
	PreRun: func(cmd *cobra.Command, args []string) {
		viper.BindPFlags(cmd.Flags())
	},
	RunE: func(cmd *cobra.Command, args []string) error {
		return runCmd(context.Background(), cmd)
	},
}

func init() {
	RunCmd.Flags().String("experiment_proto", "", "Path to experiment proto file")

	RunCmd.Flags().String("commit_sha", "", "Commit SHA to set on the experiment spec. Should be the local commit sha")
	RunCmd.Flags().StringSlice("tags", []string{}, "Tags to add to the experiments, eg 'nightly' or 'PR#XXX'")

	RunCmd.Flags().String("api_key", "", "The Pixie API key to use for deploying pixie")
	RunCmd.Flags().String("cloud_addr", "withpixie.ai:443", "The Pixie Cloud address to use for deploying pixie")

	RunCmd.Flags().String("bq_project", "pl-pixies", "The gcloud project to put bigquery results/specs in")
	RunCmd.Flags().String("bq_dataset", "px_perf", "The name of the bigquery dataset to put results/specs in")
	RunCmd.Flags().String("bq_dataset_loc", "us-west1", "The gcloud region for the bigquery dataset")

	RunCmd.Flags().String("gke_project", "pl-pixies", "The gcloud project to use for GKE clusters")
	RunCmd.Flags().String("gke_zone", "us-west1-a", "The gcloud zone to use for GKE clusters")
	RunCmd.Flags().String("gke_network", "dev", "The gcloud network to use for GKE clusters")
	RunCmd.Flags().String("gke_subnet", "", "The subnetwork to use for GKE clusters, if empty a subnetwork will be created on demand")
	RunCmd.Flags().String("gke_security_group", "gke-security-groups@pixielabs.ai", "The security group to use for GKE clusters")

	RunCmd.Flags().String("container_repo", "gcr.io/pl-dev-infra", "The container repo to push necessary containers to for the experiment")

	RootCmd.AddCommand(RunCmd)
}

func runCmd(ctx context.Context, cmd *cobra.Command) error {
	workspaceRoot, err := getWorkspaceRoot()
	if err != nil {
		log.WithError(err).Error("failed to get workspace root")
		return err
	}
	if err := os.Chdir(workspaceRoot); err != nil {
		log.WithError(err).Error("failed to change working dir to workspace root")
		return err
	}

	tags := viper.GetStringSlice("tags")
	commitSHA := viper.GetString("commit_sha")
	if commitSHA == "" {
		err = errors.New("--commit_sha is required")
		return err
	}

	pxAPIKey := viper.GetString("api_key")
	if pxAPIKey == "" {
		err = errors.New("--api_key or PX_API_KEY is required")
		return err
	}
	pxCloudAddr := viper.GetString("cloud_addr")

	specs, err := getExperimentSpecs()
	if err != nil {
		log.WithError(err).Error("failed to get experiment specs from the flags provided")
		return err
	}

	var c cluster.Provider
	c, err = gke.NewClusterProvider(&gke.ClusterOptions{
		Project:       viper.GetString("gke_project"),
		Zone:          viper.GetString("gke_zone"),
		Network:       viper.GetString("gke_network"),
		Subnet:        viper.GetString("gke_subnet"),
		SecurityGroup: viper.GetString("gke_security_group"),
	})
	if err != nil {
		log.WithError(err).Error("failed to create GKE cluster provider")
		return err
	}

	resultTable, err := createResultTable()
	if err != nil {
		log.WithError(err).Error("failed to create results table")
		return err
	}
	specTable, err := createSpecTable()
	if err != nil {
		log.WithError(err).Error("failed to create spec table")
		return err
	}

	containerRegistryRepo := viper.GetString("container_repo")

	wg := sync.WaitGroup{}
	for _, spec := range specs {
		spec.Tags = append(spec.Tags, tags...)
		spec.CommitSHA = commitSHA
		wg.Add(1)
		go func(spec *experimentpb.ExperimentSpec) {
			defer wg.Done()
			if err := runExperiment(ctx, spec, c, pxAPIKey, pxCloudAddr, resultTable, specTable, containerRegistryRepo); err != nil {
				log.WithError(err).Error("failed to run experiment")
			}
		}(spec)
	}
	wg.Wait()
	return nil
}

func runExperiment(
	ctx context.Context,
	spec *experimentpb.ExperimentSpec,
	c cluster.Provider,
	pxAPIKey string,
	pxCloudAddr string,
	resultTable *bq.Table,
	specTable *bq.Table,
	containerRegistryRepo string,
) error {
	pxCtx := pixie.NewContext(pxAPIKey, pxCloudAddr)
	r := run.NewRunner(c, pxCtx, resultTable, specTable, containerRegistryRepo)
	expID, err := uuid.NewV4()
	if err != nil {
		return err
	}
	log.WithField("experiment_id", expID).Info("Running experiment")

	if err := r.RunExperiment(ctx, expID, spec); err != nil {
		return err
	}
	return nil
}

func loadExperimentSpec(path string) (*experimentpb.ExperimentSpec, error) {
	contents, err := os.ReadFile(path)
	if err != nil {
		return nil, err
	}
	e := &experimentpb.ExperimentSpec{}
	err = proto.UnmarshalText(string(contents), e)
	if err != nil {
		return nil, err
	}
	return e, nil
}

func getExperimentSpecs() ([]*experimentpb.ExperimentSpec, error) {
	expProtoPath := viper.GetString("experiment_proto")
	if expProtoPath != "" {
		return nil, errors.New("must --experiment_proto")
	}

	spec, err := loadExperimentSpec(expProtoPath)
	if err != nil {
		return nil, err
	}
	return []*experimentpb.ExperimentSpec{spec}, nil
}

func createResultTable() (*bq.Table, error) {
	bqProject := viper.GetString("bq_project")
	bqDataset := viper.GetString("bq_dataset")
	bqDatasetLoc := viper.GetString("bq_dataset_loc")
	timePartitioning := &bigquery.TimePartitioning{
		Type:  bigquery.DayPartitioningType,
		Field: "timestamp",
	}
	return bq.NewTableForStruct(bqProject, bqDataset, bqDatasetLoc, "results", timePartitioning, bq.ResultRow{})
}

func createSpecTable() (*bq.Table, error) {
	bqProject := viper.GetString("bq_project")
	bqDataset := viper.GetString("bq_dataset")
	bqDatasetLoc := viper.GetString("bq_dataset_loc")
	var timePartitioning *bigquery.TimePartitioning
	return bq.NewTableForStruct(bqProject, bqDataset, bqDatasetLoc, "specs", timePartitioning, bq.SpecRow{})
}

func getWorkspaceRoot() (string, error) {
	workspaceDir := os.Getenv("BUILD_WORKSPACE_DIRECTORY")
	if workspaceDir != "" {
		return workspaceDir, nil
	}
	cmd := exec.Command("git", "rev-parse", "--show-toplevel")
	var stdout bytes.Buffer
	cmd.Stdout = &stdout
	cmd.Stderr = os.Stderr
	if err := cmd.Run(); err != nil {
		return "", err
	}
	return strings.Trim(stdout.String(), " \n"), nil
}
