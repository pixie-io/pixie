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
	"encoding/json"
	"errors"
	"fmt"
	"net/url"
	"os"
	"os/exec"
	"strings"
	"sync"
	"time"

	"cloud.google.com/go/bigquery"
	"github.com/cenkalti/backoff/v4"
	"github.com/gofrs/uuid"
	"github.com/gogo/protobuf/proto"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/cobra"
	"github.com/spf13/viper"
	"golang.org/x/sync/errgroup"
	v1 "k8s.io/apimachinery/pkg/apis/meta/v1"

	"px.dev/pixie/src/e2e_test/perf_tool/experimentpb"
	"px.dev/pixie/src/e2e_test/perf_tool/pkg/cluster"
	"px.dev/pixie/src/e2e_test/perf_tool/pkg/cluster/gke"
	"px.dev/pixie/src/e2e_test/perf_tool/pkg/cluster/local"
	"px.dev/pixie/src/e2e_test/perf_tool/pkg/pixie"
	"px.dev/pixie/src/e2e_test/perf_tool/pkg/run"
	"px.dev/pixie/src/e2e_test/perf_tool/pkg/suites"
	"px.dev/pixie/src/shared/bq"
)

// RunCmd launches a perf experiment by sending queueing the experiment for the px-perf cloud to handle.
var RunCmd = &cobra.Command{
	Use:   "run",
	Short: "Launch perf experiment, managed by the px-perf cluster",
	PreRun: func(cmd *cobra.Command, args []string) {
		viper.BindPFlags(cmd.Flags())
	},
	RunE: func(cmd *cobra.Command, args []string) error {
		return runCmdWithInterrruptableContext(runCmd, cmd)
	},
}

func init() {
	RunCmd.Flags().String("experiment_proto", "", "Path to experiment proto file")
	RunCmd.Flags().String("suite", "", "The suite of experiments to run")
	RunCmd.Flags().String("experiment_name", "", "The name of the experiment within the suite")

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

	RunCmd.Flags().Bool("use_local_cluster", false, "Use your local kubeconfig to get the cluster to use instead of creating a GKE cluster")

	RunCmd.Flags().Int("max_retries", 3, "Number of times to retry a failing experiment")
	RunCmd.Flags().Int("num_runs", 1, "Number of times to repeat each experiment")

	RunCmd.Flags().String("ds_report_id", "04fc228e-4f26-4e5a-bc2c-101ed86132df", "The unique ID of the datastudio report, used to print links to datastudio views")
	RunCmd.Flags().String("ds_experiment_page_id", "p_g7fj6pf4yc", "The unique ID of the datastudio experiment page, used to print links to datastudio views")
	RunCmd.Flags().Bool("pretty", false, "Pretty print output json")

	RootCmd.AddCommand(RunCmd)
}

func runCmd(ctx context.Context, cmd *cobra.Command) error {
	log.SetOutput(os.Stderr)

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
	if viper.GetBool("use_local_cluster") {
		c = &local.ClusterProvider{}
		numNodes, err := getNumNodesInCluster(ctx, c)
		if err != nil {
			log.WithError(err).Error("failed to get number of nodes in local cluster")
			return err
		}
		if len(specs) > 1 {
			err = errors.New("cannot run multiple experiments on local cluster")
			return err
		}
		// We set the cluster spec to match the number of nodes in the local cluster.
		// Otherwise, healthchecks that rely on cluster spec's num nodes will fail.
		for _, spec := range specs {
			spec.ClusterSpec.NumNodes = int32(numNodes)
		}
	} else {
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
	maxRetries := viper.GetInt("max_retries")
	numRuns := viper.GetInt("num_runs")

	eg := errgroup.Group{}
	experiments := make(chan *exp, len(specs)*numRuns)
	expCloseOnce := sync.Once{}
	defer expCloseOnce.Do(func() { close(experiments) })
	for name, spec := range specs {
		spec.Tags = append(spec.Tags, tags...)
		spec.CommitSHA = commitSHA
		for i := 0; i < numRuns; i++ {
			idx := i
			s := spec
			n := name
			eg.Go(func() error {
				expID, err := runExperiment(ctx, s, c, pxAPIKey, pxCloudAddr, resultTable, specTable, containerRegistryRepo, maxRetries)
				if err != nil {
					log.WithError(err).Error("failed to run experiment")
					return err
				}
				experiments <- &exp{
					ExperimentID:   expID,
					ExperimentName: n,
					RunIndex:       idx,
				}
				return nil
			})
		}
	}
	if err := eg.Wait(); err != nil {
		return err
	}
	expCloseOnce.Do(func() { close(experiments) })

	dsReportID := viper.GetString("ds_report_id")
	dsExperimentPageID := viper.GetString("ds_experiment_page_id")
	out := make([]*exp, 0)
	for e := range experiments {
		e.Suite = viper.GetString("suite")
		e.DatastudioURL = datastudioLink(dsReportID, dsExperimentPageID, e.ExperimentID)
		out = append(out, e)
	}
	enc := json.NewEncoder(os.Stdout)
	if viper.GetBool("pretty") {
		enc.SetIndent("", "  ")
	}
	if err := enc.Encode(out); err != nil {
		log.WithError(err).Fatal("failed to encode json output")
	}
	return nil
}

type exp struct {
	ExperimentID   uuid.UUID `json:"experiment_id"`
	Suite          string    `json:"suite"`
	ExperimentName string    `json:"experiment_name"`
	RunIndex       int       `json:"run_index,omitempty"`
	DatastudioURL  string    `json:"datastudio_url"`
}

type maxRetryBackoff struct {
	MaxRetries int
	retries    int
}

func (bo *maxRetryBackoff) NextBackOff() time.Duration {
	bo.retries++
	if bo.retries >= bo.MaxRetries {
		return backoff.Stop
	}
	return time.Duration(0)
}
func (bo *maxRetryBackoff) Reset() {
	bo.retries = 0
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
	maxRetries int,
) (uuid.UUID, error) {
	var expID uuid.UUID
	bo := &maxRetryBackoff{
		MaxRetries: maxRetries,
	}
	op := func() error {
		pxCtx := pixie.NewContext(pxAPIKey, pxCloudAddr)
		r := run.NewRunner(c, pxCtx, resultTable, specTable, containerRegistryRepo)
		var err error
		expID, err = uuid.NewV4()
		if err != nil {
			return err
		}
		log.WithField("experiment_id", expID).Info("Running experiment")

		if err := r.RunExperiment(ctx, expID, spec); err != nil {
			return err
		}
		return nil
	}
	notify := func(err error, dur time.Duration) {
		log.WithError(err).Error("failed to run experiment, retrying...")
	}
	if err := backoff.RetryNotify(op, bo, notify); err != nil {
		return uuid.UUID{}, err
	}
	return expID, nil
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

func getExperimentSpecs() (map[string]*experimentpb.ExperimentSpec, error) {
	expProtoPath := viper.GetString("experiment_proto")
	suiteName := viper.GetString("suite")
	suiteExperimentName := viper.GetString("experiment_name")

	if expProtoPath != "" {
		spec, err := loadExperimentSpec(expProtoPath)
		if err != nil {
			return nil, err
		}
		return map[string]*experimentpb.ExperimentSpec{"unnamed": spec}, nil
	}

	if suiteName != "" {
		suite, ok := suites.ExperimentSuiteRegistry[suiteName]
		if !ok {
			return nil, fmt.Errorf("no suite '%s' in ExperimentSuiteRegistry", suiteName)
		}
		suiteSpecs := suite()
		if suiteExperimentName == "" {
			return suiteSpecs, nil
		}
		spec, ok := suiteSpecs[suiteExperimentName]
		if !ok {
			return nil, fmt.Errorf("suite '%s' does not have experiment '%s'", suiteName, suiteExperimentName)
		}
		return map[string]*experimentpb.ExperimentSpec{suiteExperimentName: spec}, nil
	}

	return nil, errors.New("must specify one of --experiment_proto or --suite")
}

func createResultTable() (*bq.Table, error) {
	bqProject := viper.GetString("bq_project")
	bqDataset := viper.GetString("bq_dataset")
	bqDatasetLoc := viper.GetString("bq_dataset_loc")
	timePartitioning := &bigquery.TimePartitioning{
		Type:  bigquery.DayPartitioningType,
		Field: "timestamp",
	}
	return bq.NewTableForStruct(bqProject, bqDataset, bqDatasetLoc, "results", timePartitioning, run.ResultRow{})
}

func createSpecTable() (*bq.Table, error) {
	bqProject := viper.GetString("bq_project")
	bqDataset := viper.GetString("bq_dataset")
	bqDatasetLoc := viper.GetString("bq_dataset_loc")
	var timePartitioning *bigquery.TimePartitioning
	return bq.NewTableForStruct(bqProject, bqDataset, bqDatasetLoc, "specs", timePartitioning, run.SpecRow{})
}

func getNumNodesInCluster(ctx context.Context, c cluster.Provider) (int, error) {
	clusterCtx, cleanup, err := c.GetCluster(ctx, nil)
	if err != nil {
		return 0, err
	}
	defer cleanup()

	nl, err := clusterCtx.Clientset().CoreV1().Nodes().List(ctx, v1.ListOptions{})
	if err != nil {
		log.WithError(err).Error("failed to list cluster nodes")
	}
	return len(nl.Items), nil
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

func datastudioLink(dsReportID string, dsExperimentPageID string, expID uuid.UUID) string {
	params := fmt.Sprintf(`{"experiment_ids":"%s"}`, expID.String())
	encodedParams := url.QueryEscape(params)
	return fmt.Sprintf("https://datastudio.google.com/reporting/%s/page/%s?params=%s", dsReportID, dsExperimentPageID, encodedParams)
}
