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
	"errors"
	"fmt"
	"io"
	"os"
	"strings"
	"time"

	"github.com/fatih/color"
	"github.com/gofrs/uuid"
	"github.com/segmentio/analytics-go/v3"
	log "github.com/sirupsen/logrus"
	"github.com/spf13/cobra"
	"github.com/spf13/viper"
	"google.golang.org/grpc"
	v1 "k8s.io/api/core/v1"
	k8serrors "k8s.io/apimachinery/pkg/api/errors"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/client-go/kubernetes"
	"k8s.io/client-go/rest"

	"px.dev/pixie/src/api/proto/cloudpb"
	vztypes "px.dev/pixie/src/operator/apis/px.dev/v1alpha1"
	"px.dev/pixie/src/operator/client/versioned"
	"px.dev/pixie/src/pixie_cli/pkg/auth"
	"px.dev/pixie/src/pixie_cli/pkg/components"
	"px.dev/pixie/src/pixie_cli/pkg/pxanalytics"
	"px.dev/pixie/src/pixie_cli/pkg/pxconfig"
	"px.dev/pixie/src/pixie_cli/pkg/utils"
	"px.dev/pixie/src/pixie_cli/pkg/vizier"
	utils2 "px.dev/pixie/src/utils"
	"px.dev/pixie/src/utils/script"
	"px.dev/pixie/src/utils/shared/artifacts"
	"px.dev/pixie/src/utils/shared/k8s"
	yamlsutils "px.dev/pixie/src/utils/shared/yamls"
)

const (
	// DefaultCloudAddr is the Community Cloud address.
	DefaultCloudAddr = "withpixie.ai:443"
	// DeploySuccess is the successful deploy const.
	DeploySuccess = "successfulDeploy"
)

// BlockListedLabels are labels that we won't allow users to specify, since these are labels that we
// specify ourselves. Changing these may break the vizier update job.
var BlockListedLabels = []string{
	"vizier-bootstrap",
	"component",
	"vizier-updater-dep",
	"app",
}

func init() {
	DeployCmd.Flags().StringP("extract_yaml", "e", "", "Directory to extract the Pixie yamls to")
	DeployCmd.Flags().StringP("vizier_version", "v", "", "Pixie version to deploy")
	DeployCmd.Flags().BoolP("check", "c", true, "Check whether the cluster can run Pixie")
	DeployCmd.Flags().BoolP("check_only", "", false, "Only run check and exit.")
	DeployCmd.Flags().StringP("namespace", "n", "pl", "The namespace to deploy Vizier to")
	DeployCmd.Flags().StringP("deploy_key", "k", "", "The deploy key to use to deploy Pixie")
	DeployCmd.Flags().BoolP("use_etcd_operator", "o", false, "Whether to use the operator for etcd instead of the statefulset")
	DeployCmd.Flags().StringP("labels", "l", "", "Custom labels to apply to Pixie resources")
	DeployCmd.Flags().StringP("annotations", "t", "", "Custom annotations to apply to Pixie resources")
	DeployCmd.Flags().StringP("cluster_name", "u", "", "The name for your cluster. Otherwise, the name will be taken from the current kubeconfig.")
	DeployCmd.Flags().StringP("pem_memory_limit", "p", "", "The memory limit to specify for the PEMs, otherwise a default is used.")
	DeployCmd.Flags().StringP("pem_memory_request", "r", "", "The memory request to specify for the PEMs, otherwise a default is used.")
	DeployCmd.Flags().StringArray("patches", []string{}, "Custom patches to apply to Pixie yamls, for example: 'vizier-pem:{\"spec\":{\"template\":{\"spec\":{\"nodeSelector\":{\"pixie\": \"allowed\"}}}}}'")
	DeployCmd.Flags().String("pem_flags", "", "Flags to be set on the PEM.")
	DeployCmd.Flags().String("registry", "", "The custom image registry to use rather than Pixie's default (gcr.io).")
	DeployCmd.Flags().BoolP("disable_auto_update", "d", false, "Disable the auto-update feature for the vizier client.")

	// Flags for deploying OLM.
	DeployCmd.Flags().String("operator_version", "", "Operator version to deploy")
	DeployCmd.Flags().Bool("deploy_olm", true, "Whether to deploy Operator Lifecycle Manager. OLM is required. This should only be false if OLM is already deployed on the cluster (either manually or through another application). Note: OLM is deployed by default on Openshift clusters.")
	DeployCmd.Flags().String("olm_namespace", "olm", "The namespace to use for the Operator Lifecycle Manager")
	DeployCmd.Flags().String("olm_operator_namespace", "px-operator", "The namespace to use for the Pixie operator")
	DeployCmd.Flags().String("data_access", "Full", "Data access level defines the level of data that may be accessed when executing a script on the cluster. Options: 'Full' and 'Restricted'")
	DeployCmd.Flags().Uint32("datastream_buffer_size", 0, "Internal data collector parameters: the maximum size of a data stream buffer retained between cycles.")
	DeployCmd.Flags().Uint32("datastream_buffer_spike_size", 0, "Internal data collector parameters: the maximum temporary size of a data stream buffer before processing.")
	// Super secret flags for Pixies.
	DeployCmd.Flags().MarkHidden("namespace")
}

// DeployCmd is the "deploy" command.
var DeployCmd = &cobra.Command{
	Use:   "deploy",
	Short: "Deploys Pixie on the current K8s cluster",
	PreRun: func(cmd *cobra.Command, args []string) {
		viper.BindPFlag("extract_yaml", cmd.Flags().Lookup("extract_yaml"))
		viper.BindPFlag("vizier_version", cmd.Flags().Lookup("vizier_version"))
		viper.BindPFlag("check", cmd.Flags().Lookup("check"))
		viper.BindPFlag("check_only", cmd.Flags().Lookup("check_only"))
		viper.BindPFlag("namespace", cmd.Flags().Lookup("namespace"))
		viper.BindPFlag("deploy_key", cmd.Flags().Lookup("deploy_key"))
		viper.BindPFlag("use_etcd_operator", cmd.Flags().Lookup("use_etcd_operator"))
		viper.BindPFlag("labels", cmd.Flags().Lookup("labels"))
		viper.BindPFlag("annotations", cmd.Flags().Lookup("annotations"))
		viper.BindPFlag("cluster_name", cmd.Flags().Lookup("cluster_name"))
		viper.BindPFlag("pem_memory_limit", cmd.Flags().Lookup("pem_memory_limit"))
		viper.BindPFlag("pem_memory_request", cmd.Flags().Lookup("pem_memory_request"))
		viper.BindPFlag("patches", cmd.Flags().Lookup("patches"))
		viper.BindPFlag("pem_flags", cmd.Flags().Lookup("pem_flags"))
		viper.BindPFlag("operator_version", cmd.Flags().Lookup("operator_version"))
		viper.BindPFlag("deploy_olm", cmd.Flags().Lookup("deploy_olm"))
		viper.BindPFlag("olm_namespace", cmd.Flags().Lookup("olm_namespace"))
		viper.BindPFlag("olm_operator_namespace", cmd.Flags().Lookup("olm_operator_namespace"))
		viper.BindPFlag("data_access", cmd.Flags().Lookup("data_access"))
		viper.BindPFlag("datastream_buffer_size", cmd.Flags().Lookup("datastream_buffer_size"))
		viper.BindPFlag("datastream_buffer_spike_size", cmd.Flags().Lookup("datastream_buffer_spike_size"))
		viper.BindPFlag("disable_auto_update", cmd.Flags().Lookup("disable_auto_update"))
	},
	PostRun: func(cmd *cobra.Command, args []string) {
		if cmd.Annotations["status"] != DeploySuccess {
			return
		}

		p := func(s string, a ...interface{}) {
			fmt.Fprintf(os.Stderr, s, a...)
		}
		u := color.New(color.Underline).Sprintf
		b := color.New(color.Bold).Sprintf
		g := color.GreenString

		cloudAddr := viper.GetString("cloud_addr")
		docsAddr := cloudAddr
		if cloudAddr != DefaultCloudAddr {
			docsAddr = "px.dev"
		}

		fmt.Fprint(os.Stderr, "\n")
		p(color.CyanString("==> ") + b("Next Steps:\n"))
		p("\nRun some scripts using the %s cli. For example: \n", g("px"))
		p("- %s : to show pre-installed scripts.\n", g("px script list"))
		p("- %s : to run service info for sock-shop demo application (service selection coming soon!).\n",
			g("px run %s", script.ServiceStatsScript))
		p("\nCheck out our docs: %s.\n", u("https://docs.%s", docsAddr))
		p("\nVisit : %s to use Pixie's UI.\n", u("https://work.%s", cloudAddr))
	},
	Run: runDeployCmd,
}

type taskWrapper struct {
	name string
	run  func() error
}

func newTaskWrapper(name string, run func() error) *taskWrapper {
	return &taskWrapper{
		name,
		run,
	}
}

func (t *taskWrapper) Name() string {
	return t.name
}

func (t *taskWrapper) Run() error {
	return t.run()
}

func newArtifactTrackerClient(conn *grpc.ClientConn) cloudpb.ArtifactTrackerClient {
	return cloudpb.NewArtifactTrackerClient(conn)
}

func getLatestVizierVersion(conn *grpc.ClientConn) (string, error) {
	client := newArtifactTrackerClient(conn)

	req := &cloudpb.GetArtifactListRequest{
		ArtifactName: "vizier",
		ArtifactType: cloudpb.AT_CONTAINER_SET_YAMLS,
		Limit:        1,
	}
	ctxWithCreds := auth.CtxWithCreds(context.Background())
	resp, err := client.GetArtifactList(ctxWithCreds, req)
	if err != nil {
		return "", err
	}

	if len(resp.Artifact) != 1 {
		return "", errors.New("Could not find Vizier artifact")
	}

	return resp.Artifact[0].VersionStr, nil
}

func getLatestOperatorVersion(conn *grpc.ClientConn) (string, error) {
	client := newArtifactTrackerClient(conn)

	req := &cloudpb.GetArtifactListRequest{
		ArtifactName: "operator",
		ArtifactType: cloudpb.AT_CONTAINER_SET_TEMPLATE_YAMLS,
		Limit:        1,
	}
	ctxWithCreds := auth.CtxWithCreds(context.Background())
	resp, err := client.GetArtifactList(ctxWithCreds, req)
	if err != nil {
		return "", err
	}

	if len(resp.Artifact) != 1 {
		return "", errors.New("Could not find Operator artifact")
	}

	return resp.Artifact[0].VersionStr, nil
}

func runDeployCmd(cmd *cobra.Command, args []string) {
	check, _ := cmd.Flags().GetBool("check")
	checkOnly, _ := cmd.Flags().GetBool("check_only")
	extractPath, _ := cmd.Flags().GetString("extract_yaml")

	// OLM flags.
	deployOLM, _ := cmd.Flags().GetBool("deploy_olm")
	olmNamespace, _ := cmd.Flags().GetString("olm_namespace")
	olmOperatorNamespace, _ := cmd.Flags().GetString("olm_operator_namespace")

	deployKey, _ := cmd.Flags().GetString("deploy_key")
	useEtcdOperator, _ := cmd.Flags().GetBool("use_etcd_operator")
	disableAutoUpdate, _ := cmd.Flags().GetBool("disable_auto_update")
	customLabels, _ := cmd.Flags().GetString("labels")
	customAnnotations, _ := cmd.Flags().GetString("annotations")
	pemMemoryLimit, _ := cmd.Flags().GetString("pem_memory_limit")
	pemMemoryRequest, _ := cmd.Flags().GetString("pem_memory_request")
	pemFlags, _ := cmd.Flags().GetString("pem_flags")
	patches, _ := cmd.Flags().GetStringArray("patches")
	dataAccess, _ := cmd.Flags().GetString("data_access")
	datastreamBufferSize, _ := cmd.Flags().GetUint32("datastream_buffer_size")
	datastreamBufferSpikeSize, _ := cmd.Flags().GetUint32("datastream_buffer_spike_size")
	registry, _ := cmd.Flags().GetString("registry")

	labelMap := make(map[string]string)
	if customLabels != "" {
		lm, err := k8s.KeyValueStringToMap(customLabels)
		if err != nil {
			utils.WithError(err).Fatal("--labels must be specified through the following format: label1=value1,label2=value2")
		}
		labelMap = lm
	}
	// Check that none of the labels override ours.
	for _, l := range BlockListedLabels {
		if _, ok := labelMap[l]; ok {
			joinedLabels := strings.Join(BlockListedLabels, ", ")
			utils.Fatalf("Custom labels must not be one of: %s.", joinedLabels)
		}
	}
	annotationMap := make(map[string]string)
	if customAnnotations != "" {
		am, err := k8s.KeyValueStringToMap(customAnnotations)
		if err != nil {
			utils.WithError(err).Fatal("--annotations must be specified through the following format: annotation1=value1,annotation2=value2")
		}
		annotationMap = am
	}
	patchesMap := make(map[string]string)
	if len(patches) != 0 {
		for _, p := range patches {
			colon := strings.Index(p, ":")
			if colon == -1 {
				continue
			}
			patchesMap[p[:colon]] = p[colon+1:]
		}
	}
	pemFlagsMap := make(map[string]string)
	if pemFlags != "" {
		pf, err := k8s.KeyValueStringToMap(pemFlags)
		if err != nil {
			utils.WithError(err).Fatal("--pem_flags must be specified through the following format: PL_KEY_1=value1,PL_KEY_2=value2")
		}
		pemFlagsMap = pf
	}
	dataCollectorParams := make(map[string]interface{})
	dataCollectorParams["customPEMFlags"] = pemFlagsMap
	if datastreamBufferSize != 0 {
		dataCollectorParams["datastreamBufferSize"] = datastreamBufferSize
	}
	if datastreamBufferSpikeSize != 0 {
		dataCollectorParams["datastreamBufferSpikeSize"] = datastreamBufferSpikeSize
	}

	castedDataAccess := vztypes.DataAccessLevel(dataAccess)
	if castedDataAccess != vztypes.DataAccessFull && castedDataAccess != vztypes.DataAccessRestricted {
		utils.Fatal("--data_access must be a valid data access level")
	}

	if deployKey == "" && extractPath != "" {
		utils.Fatal("--deploy_key must be specified when running with --extract_yaml. Please run px deploy-key create.")
	}

	if (check || checkOnly) && extractPath == "" {
		_ = pxanalytics.Client().Enqueue(&analytics.Track{
			UserId: pxconfig.Cfg().UniqueClientID,
			Event:  "Cluster Check Run",
		})

		err := utils.RunDefaultClusterChecks()
		if err != nil {
			_ = pxanalytics.Client().Enqueue(&analytics.Track{
				UserId: pxconfig.Cfg().UniqueClientID,
				Event:  "Cluster Check Failed",
				Properties: analytics.NewProperties().
					Set("error", err.Error()),
			})
			utils.WithError(err).Fatal("Check pre-check has failed. To bypass pass in --check=false.")
		}

		if checkOnly {
			log.Info("All Required Checks Passed!")
			os.Exit(0)
		}

		err = utils.RunExtraClusterChecks()
		if err != nil {
			clusterOk := components.YNPrompt("Some cluster checks failed. Pixie may not work properly on your cluster. Continue with deploy?", true)
			if !clusterOk {
				utils.Error("Deploy cancelled. Aborting...")
				return
			}
		}
	}

	namespace, _ := cmd.Flags().GetString("namespace")
	devCloudNS := viper.GetString("dev_cloud_namespace")
	cloudAddr := viper.GetString("cloud_addr")

	// Get grpc connection to cloud.
	cloudConn, err := utils.GetCloudClientConnection(cloudAddr)
	if err != nil {
		// Using log.Fatal rather than CLI log in order to track this unexpected error in Sentry.
		log.WithError(err).Fatalln("Failed to get grpc connection to cloud")
	}

	versionString := viper.GetString("vizier_version")
	if len(versionString) == 0 {
		// Fetch latest version.
		versionString, err = getLatestVizierVersion(cloudConn)
		if err != nil {
			// Using log.Fatal rather than CLI log in order to track this unexpected error in Sentry.
			log.WithError(err).Fatal("Failed to fetch Vizier versions")
		}
	}
	utils.Infof("Installing Vizier version: %s", versionString)

	operatorVersion := viper.GetString("operator_version")
	if len(operatorVersion) == 0 {
		operatorVersion, err = getLatestOperatorVersion(cloudConn)
		if err != nil {
			// Using log.Fatal rather than CLI log in order to track this unexpected error in Sentry.
			log.WithError(err).Fatal("Failed to fetch Operator versions")
		}
	}
	olmBundleChannel := "stable"
	if strings.Contains(operatorVersion, "-") {
		olmBundleChannel = "dev"
	}

	// Get deploy key, if not already specified.
	var deployKeyID string
	if deployKey == "" {
		deployKeyID, deployKey, err = generateDeployKey(cloudAddr, "Auto-generated by the Pixie CLI")
		if err != nil {
			// Using log.Fatal rather than CLI log in order to track this unexpected error in Sentry.
			log.WithError(err).Fatal("Failed to generate deployment key")
		}
		defer func() {
			err := deleteDeployKey(cloudAddr, uuid.FromStringOrNil(deployKeyID))
			if err != nil {
				log.WithError(err).Info("Failed to delete generated deploy key")
			}
		}()
	}

	kubeConfig := k8s.GetConfig()
	kubeAPIConfig := k8s.GetClientAPIConfig()
	clientset := k8s.GetClientset(kubeConfig)
	vzClient, err := versioned.NewForConfig(kubeConfig)
	if err != nil {
		log.WithError(err).Fatal("Could not start vizier client")
	}

	utils.Infof("Generating YAMLs for Pixie")

	templatedYAMLs, err := artifacts.FetchOperatorTemplates(cloudConn, operatorVersion)
	if err != nil {
		log.WithError(err).Fatal("Could not fetch Vizier YAMLs")
	}

	clusterName, _ := cmd.Flags().GetString("cluster_name")
	if clusterName == "" {
		clusterName = kubeAPIConfig.CurrentContext
	}

	tmplCloudAddr := cloudAddr
	if devCloudNS != "" {
		tmplCloudAddr = fmt.Sprintf("api-service.%s.svc.cluster.local:51200", devCloudNS)
	}

	// Fill in template values.
	tmplArgs := &yamlsutils.YAMLTmplArguments{
		Values: &map[string]interface{}{
			"deployOLM":            deployOLM,
			"olmNamespace":         olmNamespace,
			"olmBundleChannel":     olmBundleChannel,
			"olmOperatorNamespace": olmOperatorNamespace,
			"name":                 "pixie",
			"version":              versionString,
			"deployKey":            deployKey,
			"cloudAddr":            tmplCloudAddr,
			"clusterName":          clusterName,
			"disableAutoUpdate":    disableAutoUpdate,
			"useEtcdOperator":      useEtcdOperator,
			"devCloudNamespace":    devCloudNS,
			"pemMemoryLimit":       pemMemoryLimit,
			"pemMemoryRequest":     pemMemoryRequest,
			"pod": &map[string]interface{}{
				"annotations": annotationMap,
				"labels":      labelMap,
			},
			"patches":             patchesMap,
			"dataAccess":          castedDataAccess,
			"dataCollectorParams": dataCollectorParams,
			"registry":            registry,
		},
		Release: &map[string]interface{}{
			"Namespace": namespace,
		},
	}

	yamls, err := yamlsutils.ExecuteTemplatedYAMLs(templatedYAMLs, tmplArgs)
	if err != nil {
		log.WithError(err).Fatal("Failed to fill in templated deployment YAMLs")
	}

	// If extract_path is specified, write out yamls to file.
	if extractPath != "" {
		if err := yamlsutils.ExtractYAMLs(yamls, extractPath, "pixie_yamls", yamlsutils.MultiFileExtractYAMLFormat); err != nil {
			log.WithError(err).Fatal("failed to extract deployment YAMLs")
		}
		return
	}

	// Map from the YAML name to the YAML contents.
	yamlMap := make(map[string]string)
	for _, y := range yamls {
		yamlMap[y.Name] = y.YAML
	}

	_ = pxanalytics.Client().Enqueue(&analytics.Track{
		UserId: pxconfig.Cfg().UniqueClientID,
		Event:  "Deploy Initiated",
		Properties: analytics.NewProperties().
			Set("cloud_addr", cloudAddr),
	})

	_ = pxanalytics.Client().Enqueue(&analytics.Track{
		UserId: pxconfig.Cfg().UniqueClientID,
		Event:  "Deploy Started",
		Properties: analytics.NewProperties().
			Set("cloud_addr", cloudAddr),
	})

	currentCluster := kubeAPIConfig.CurrentContext
	utils.Infof("Deploying Pixie to the following cluster: %s", currentCluster)
	clusterOk := components.YNPrompt("Is the cluster correct?", true)
	if !clusterOk {
		utils.Error("Cluster is not correct. Aborting.")
		return
	}

	// Get the number of nodes.
	numNodes, err := getNumNodes(clientset)
	if err != nil {
		utils.Error(err.Error())
	}
	if numNodes == 0 {
		utils.Error("Cluster has no nodes. Try deploying Pixie to a cluster with at least one node.")
		return
	}

	utils.Infof("Found %v nodes", numNodes)

	clusterID := deploy(cloudConn, clientset, vzClient, kubeConfig, yamlMap, deployOLM, olmNamespace, olmOperatorNamespace, namespace)

	waitForHealthCheck(cloudAddr, clusterID, clientset, namespace, numNodes)

	cmd.Annotations = make(map[string]string)
	cmd.Annotations["status"] = DeploySuccess
}

func deploy(cloudConn *grpc.ClientConn, clientset *kubernetes.Clientset, vzClient *versioned.Clientset, kubeConfig *rest.Config, yamlMap map[string]string, deployOLM bool, olmNs, olmOpNs, namespace string) uuid.UUID {
	olmCRDJob := newTaskWrapper("Installing OLM CRDs", func() error {
		return retryDeploy(clientset, kubeConfig, yamlMap["olm_crd"])
	})
	olmJob := newTaskWrapper("Deploying OLM", func() error {
		return retryDeploy(clientset, kubeConfig, yamlMap["olm"])
	})

	olmPxJob := newTaskWrapper("Deploying Pixie OLM Namespace", func() error {
		return retryDeploy(clientset, kubeConfig, yamlMap["px_olm"])
	})

	olmCatalogJob := newTaskWrapper("Deploying OLM Catalog", func() error {
		return retryDeploy(clientset, kubeConfig, yamlMap["catalog"])
	})
	olmSubscriptionJob := newTaskWrapper("Deploying OLM Subscription", func() error {
		return retryDeploy(clientset, kubeConfig, yamlMap["subscription"])
	})

	namespaceJob := newTaskWrapper("Creating namespace", func() error {
		// Create namespace, if needed.
		ns := &v1.Namespace{}
		ns.SetGroupVersionKind(v1.SchemeGroupVersion.WithKind("Namespace"))
		ns.Name = namespace

		_, err := clientset.CoreV1().Namespaces().Create(context.Background(), ns, metav1.CreateOptions{})
		if err != nil && k8serrors.IsAlreadyExists(err) {
			return nil
		}
		return err
	})

	vzCRDJob := newTaskWrapper("Installing Vizier CRD", func() error {
		// Delete existing CRD, if any.
		_ = vzClient.PxV1alpha1().Viziers(namespace).Delete(context.Background(), "pixie", metav1.DeleteOptions{})

		return retryDeploy(clientset, kubeConfig, yamlMap["vizier_crd"])
	})
	vzJob := newTaskWrapper("Deploying Vizier", func() error {
		return retryDeploy(clientset, kubeConfig, yamlMap["vizier"])
	})

	var clusterID uuid.UUID
	waitJob := newTaskWrapper("Waiting for Cloud Connector to come online", func() error {
		ctx, cancel := context.WithTimeout(context.Background(), 5*time.Minute)
		defer cancel()

		t := time.NewTicker(2 * time.Second)
		defer t.Stop()
		clusterIDExists := false
		for !clusterIDExists { // Wait for secret to be updated with clusterID.
			select {
			case <-ctx.Done():
				// Using log.Fatal rather than CLI log in order to track this unexpected error in Sentry.
				log.Fatal("Timed out waiting for cluster ID assignment")
			case <-t.C:
				s := k8s.GetSecret(clientset, namespace, "pl-cluster-secrets")
				if s == nil {
					continue
				}
				if cID, ok := s.Data["cluster-id"]; ok {
					clusterID = uuid.FromStringOrNil(string(cID))
					clusterIDExists = true
				}
			}
		}

		return waitForCluster(ctx, cloudConn, clusterID)
	})

	deployJobs := []utils.Task{
		vzCRDJob, olmPxJob, olmCatalogJob, olmSubscriptionJob, namespaceJob, vzJob, waitJob,
	}

	if deployOLM {
		deployJobs = []utils.Task{
			olmCRDJob, olmJob, olmPxJob, vzCRDJob, olmCatalogJob, olmSubscriptionJob, namespaceJob, vzJob, waitJob,
		}
	}

	jr := utils.NewSerialTaskRunner(deployJobs)
	err := jr.RunAndMonitor()
	if err != nil {
		_ = pxanalytics.Client().Enqueue(&analytics.Track{
			UserId: pxconfig.Cfg().UniqueClientID,
			Event:  "Deploy Failure",
			Properties: analytics.NewProperties().
				Set("err", err.Error()),
		})
		// Using log.Fatal rather than CLI log in order to track this error in Sentry.
		log.WithError(err).Fatal("Failed to deploy Vizier")
	}

	return clusterID
}

func runSimpleHealthCheckScript(cloudAddr string, clusterID uuid.UUID) error {
	v, err := vizier.ConnectionToVizierByID(cloudAddr, clusterID)
	br := mustCreateBundleReader()
	if err != nil {
		return err
	}
	execScript := br.MustGetScript(script.AgentStatusScript)

	ctx, cancel := context.WithTimeout(context.Background(), 2*time.Second)
	defer cancel()

	resp, err := v.ExecuteScriptStream(ctx, execScript, nil)
	if err != nil {
		return err
	}

	// TODO(zasgar): Make this use the Null output. We can't right now
	// because of fatal message on vizier failure.
	errCh := make(chan error)
	// Eat all responses.
	go func() {
		for {
			select {
			case <-ctx.Done():
				if ctx.Err() != nil {
					errCh <- ctx.Err()
					return
				}
				errCh <- nil
				return
			case msg := <-resp:
				if msg == nil {
					errCh <- nil
					return
				}
				if msg.Err != nil {
					if msg.Err == io.EOF {
						errCh <- nil
						return
					}
					errCh <- msg.Err
					return
				}
				if msg.Resp.Status != nil && msg.Resp.Status.Code != 0 {
					errCh <- errors.New(msg.Resp.Status.Message)
				}
				// Eat messages.
			}
		}
	}()

	err = <-errCh
	return err
}

func waitForHealthCheckTaskGenerator(cloudAddr string, clusterID uuid.UUID) func() error {
	return func() error {
		timeout := time.NewTimer(5 * time.Minute)
		defer timeout.Stop()
		for {
			select {
			case <-timeout.C:
				return errors.New("timeout waiting for healthcheck  (it is possible that Pixie stabilized after the healthcheck timeout. To check if Pixie successfully deployed, run `px debug pods`)")
			default:
				err := runSimpleHealthCheckScript(cloudAddr, clusterID)
				if err == nil {
					return nil
				}
				time.Sleep(5 * time.Second)
			}
		}
	}
}

func waitForHealthCheck(cloudAddr string, clusterID uuid.UUID, clientset *kubernetes.Clientset, namespace string, numNodes int) {
	utils.Info("Waiting for Pixie to pass healthcheck")

	healthCheckJobs := []utils.Task{
		newTaskWrapper("Wait for PEMs/Kelvin", func() error {
			return waitForPems(clientset, namespace, numNodes)
		}),
		newTaskWrapper("Wait for healthcheck", waitForHealthCheckTaskGenerator(cloudAddr, clusterID)),
	}

	hc := utils.NewSerialTaskRunner(healthCheckJobs)
	err := hc.RunAndMonitor()
	if err != nil {
		_ = pxanalytics.Client().Enqueue(&analytics.Track{
			UserId: pxconfig.Cfg().UniqueClientID,
			Event:  "Deploy Healthcheck Failed",
			Properties: analytics.NewProperties().
				Set("err", err.Error()),
		})
		utils.WithError(err).Fatal("Failed Pixie healthcheck")
	}
	_ = pxanalytics.Client().Enqueue(&analytics.Track{
		UserId: pxconfig.Cfg().UniqueClientID,
		Event:  "Deploy Healthcheck Passed",
	})
}

func waitForCluster(ctx context.Context, conn *grpc.ClientConn, clusterID uuid.UUID) error {
	client := cloudpb.NewVizierClusterInfoClient(conn)

	req := &cloudpb.GetClusterInfoRequest{
		ID: utils2.ProtoFromUUID(clusterID),
	}

	ctxWithCreds := auth.CtxWithCreds(context.Background())
	t := time.NewTicker(2 * time.Second)
	defer t.Stop()
	for {
		select {
		case <-t.C:
			resp, err := client.GetClusterInfo(ctxWithCreds, req)
			if err != nil {
				return err
			}
			if len(resp.Clusters) > 0 && resp.Clusters[0].Status != cloudpb.CS_DISCONNECTED {
				return nil
			}
		case <-ctx.Done():
			return errors.New("context cancelled waiting for cluster to come online")
		}
	}
}

func initiateUpdate(ctx context.Context, conn *grpc.ClientConn, clusterID uuid.UUID, version string, redeployEtcd bool) error {
	client := cloudpb.NewVizierClusterInfoClient(conn)

	req := &cloudpb.UpdateOrInstallClusterRequest{
		ClusterID:    utils2.ProtoFromUUID(clusterID),
		Version:      version,
		RedeployEtcd: redeployEtcd,
	}

	ctxWithCreds := auth.CtxWithCreds(context.Background())
	resp, err := client.UpdateOrInstallCluster(ctxWithCreds, req)
	if err != nil {
		return err
	}
	if !resp.UpdateStarted {
		return errors.New("failed to start install process")
	}
	return nil
}

func retryDeploy(clientset *kubernetes.Clientset, config *rest.Config, yamlContents string) error {
	tries := 12
	var err error
	for tries > 0 {
		err = k8s.ApplyYAML(clientset, config, "", strings.NewReader(yamlContents), false)
		if err == nil {
			return nil
		}

		if err != nil && k8serrors.IsAlreadyExists(err) {
			return nil
		}
		time.Sleep(5 * time.Second)
		tries--
	}
	if tries == 0 {
		return err
	}
	return nil
}

func isPodUnschedulable(podStatus *v1.PodStatus) bool {
	for _, cond := range podStatus.Conditions {
		if cond.Reason == "Unschedulable" {
			return true
		}
	}
	return false
}

func podUnschedulableMessage(podStatus *v1.PodStatus) string {
	for _, cond := range podStatus.Conditions {
		if cond.Reason == "Unschedulable" {
			return cond.Message
		}
	}
	return ""
}

func getNumNodes(clientset *kubernetes.Clientset) (int, error) {
	nodes, err := clientset.CoreV1().Nodes().List(context.Background(), metav1.ListOptions{})
	if err != nil {
		return 0, err
	}
	return len(nodes.Items), nil
}

var empty struct{}

// waitForPems waits for the Vizier's Proxy service to be ready with an external IP.
func waitForPems(clientset *kubernetes.Clientset, namespace string, expectedPods int) error {
	// Watch for pod updates.
	watcher, err := clientset.CoreV1().Pods(namespace).Watch(context.Background(), metav1.ListOptions{})
	if err != nil {
		return err
	}

	failedSchedulingPods := make(map[string]string)
	successfulPods := make(map[string]struct{})
	for c := range watcher.ResultChan() {
		pod := c.Object.(*v1.Pod)
		name, ok := pod.Labels["name"]
		if !ok {
			continue
		}

		// Skip any pods that are not vizier-pems.
		if name != "vizier-pem" {
			continue
		}

		switch pod.Status.Phase {
		case "Pending":
			if isPodUnschedulable(&pod.Status) {
				failedSchedulingPods[pod.Name] = podUnschedulableMessage(&pod.Status)
			}

		case "Running":
			successfulPods[pod.Name] = empty
		default:
			return fmt.Errorf("unexpected status for PEM '%s': '%v'", pod.Name, pod.Status.Phase)
		}

		if len(successfulPods) == expectedPods {
			return nil
		}
		if len(successfulPods)+len(failedSchedulingPods) == expectedPods {
			failedPems := make([]string, 0)
			for k, v := range failedSchedulingPods {
				failedPems = append(failedPems, fmt.Sprintf("'%s': '%s'", k, v))
			}

			return fmt.Errorf("Failed to schedule pems:\n%s", strings.Join(failedPems, "\n"))
		}
	}
	return nil
}
