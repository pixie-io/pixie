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
	log "github.com/sirupsen/logrus"
	"github.com/spf13/cobra"
	"github.com/spf13/viper"
	"google.golang.org/grpc"
	"gopkg.in/segmentio/analytics-go.v3"
	v1 "k8s.io/api/core/v1"
	k8serrors "k8s.io/apimachinery/pkg/api/errors"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/client-go/kubernetes"
	"k8s.io/client-go/rest"

	"px.dev/pixie/src/api/proto/cloudpb"
	"px.dev/pixie/src/pixie_cli/pkg/auth"
	"px.dev/pixie/src/pixie_cli/pkg/components"
	"px.dev/pixie/src/pixie_cli/pkg/pxanalytics"
	"px.dev/pixie/src/pixie_cli/pkg/pxconfig"
	"px.dev/pixie/src/pixie_cli/pkg/script"
	"px.dev/pixie/src/pixie_cli/pkg/utils"
	"px.dev/pixie/src/pixie_cli/pkg/vizier"
	utils2 "px.dev/pixie/src/utils"
	"px.dev/pixie/src/utils/shared/artifacts"
	"px.dev/pixie/src/utils/shared/k8s"
	yamlsutils "px.dev/pixie/src/utils/shared/yamls"
	vizieryamls "px.dev/pixie/src/utils/template_generator/vizier_yamls"
)

const (
	// DefaultClassAnnotationKey is the key in the annotation map which indicates
	// a storage class is default.
	DefaultClassAnnotationKey = "storageclass.kubernetes.io/is-default-class"
)

// BlockListedLabels are labels that we won't allow users to specify, since these are labels that we
// specify ourselves. Changing these may break the vizier update job.
var BlockListedLabels = []string{
	"vizier-bootstrap",
	"component",
	"vizier-updater-dep",
	"app",
}

// DeployCmd is the "deploy" command.
var DeployCmd = &cobra.Command{
	Use:   "deploy",
	Short: "Deploys Pixie on the current K8s cluster",
	PostRun: func(cmd *cobra.Command, args []string) {
		extractPath, _ := cmd.Flags().GetString("extract_yaml")
		if extractPath != "" {
			return
		}

		p := func(s string, a ...interface{}) {
			fmt.Fprintf(os.Stderr, s, a...)
		}
		u := color.New(color.Underline).Sprintf
		b := color.New(color.Bold).Sprintf
		g := color.GreenString

		cloudAddr := viper.GetString("cloud_addr")

		fmt.Fprint(os.Stderr, "\n")
		p(color.CyanString("==> ") + b("Next Steps:\n"))
		p("\nRun some scripts using the %s cli. For example: \n", g("px"))
		p("- %s : to show pre-installed scripts.\n", g("px script list"))
		p("- %s : to run service info for sock-shop demo application (service selection coming soon!).\n",
			g("px run %s", script.ServiceStatsScript))
		p("\nCheck out our docs: %s.\n", u("https://work.%s/docs", cloudAddr))
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

func init() {
	DeployCmd.Flags().StringP("extract_yaml", "e", "", "Directory to extract the Pixie yamls to")
	viper.BindPFlag("extract_yaml", DeployCmd.Flags().Lookup("extract_yaml"))

	DeployCmd.Flags().StringP("vizier_version", "v", "", "Pixie version to deploy")
	viper.BindPFlag("vizier_version", DeployCmd.Flags().Lookup("vizier_version"))

	DeployCmd.Flags().BoolP("check", "c", true, "Check whether the cluster can run Pixie")
	viper.BindPFlag("check", DeployCmd.Flags().Lookup("check"))

	DeployCmd.Flags().BoolP("check_only", "", false, "Only run check and exit.")
	viper.BindPFlag("check_only", DeployCmd.Flags().Lookup("check_only"))

	DeployCmd.Flags().StringP("namespace", "n", "pl", "The namespace to deploy Vizier to")
	viper.BindPFlag("namespace", DeployCmd.Flags().Lookup("namespace"))

	DeployCmd.Flags().StringP("deploy_key", "k", "", "The deploy key to use to deploy Pixie")
	viper.BindPFlag("deploy_key", DeployCmd.Flags().Lookup("deploy_key"))

	DeployCmd.Flags().BoolP("use_etcd_operator", "o", false, "Whether to use the operator for etcd instead of the statefulset")
	viper.BindPFlag("use_etcd_operator", DeployCmd.Flags().Lookup("use_etcd_operator"))

	DeployCmd.Flags().StringP("labels", "l", "", "Custom labels to apply to Pixie resources")
	viper.BindPFlag("labels", DeployCmd.Flags().Lookup("labels"))

	DeployCmd.Flags().StringP("annotations", "t", "", "Custom annotations to apply to Pixie resources")
	viper.BindPFlag("annotations", DeployCmd.Flags().Lookup("annotations"))

	DeployCmd.Flags().StringP("cluster_name", "u", "", "The name for your cluster. Otherwise, the name will be taken from the current kubeconfig.")
	viper.BindPFlag("cluster_name", DeployCmd.Flags().Lookup("cluster_name"))

	DeployCmd.Flags().StringP("pem_memory_limit", "p", "", "The memory limit to specify for the PEMS, otherwise a default is used.")
	viper.BindPFlag("pem_memory_limit", DeployCmd.Flags().Lookup("pem_memory_limit"))

	// Super secret flags for Pixies.
	DeployCmd.Flags().MarkHidden("namespace")
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

func setTemplateConfigValues(currentCluster string, tmplValues *vizieryamls.VizierTmplValues, cloudAddr, devCloudNS, clusterName string) {
	yamlCloudAddr := cloudAddr
	updateCloudAddr := cloudAddr
	// devCloudNamespace implies we are running in a dev enivironment and we should attach to
	// vzconn in that namespace.
	if devCloudNS != "" {
		yamlCloudAddr = fmt.Sprintf("vzconn-service.%s.svc.cluster.local:51600", devCloudNS)
		updateCloudAddr = fmt.Sprintf("api-service.%s.svc.cluster.local:51200", devCloudNS)
	}

	tmplValues.CloudAddr = yamlCloudAddr
	tmplValues.CloudUpdateAddr = updateCloudAddr

	if clusterName == "" { // Only record cluster name if we are deploying directly to the current cluster.
		clusterName = currentCluster
	}
	tmplValues.ClusterName = clusterName
}

func runDeployCmd(cmd *cobra.Command, args []string) {
	check, _ := cmd.Flags().GetBool("check")
	checkOnly, _ := cmd.Flags().GetBool("check_only")
	extractPath, _ := cmd.Flags().GetString("extract_yaml")
	deployKey, _ := cmd.Flags().GetString("deploy_key")
	useEtcdOperator, _ := cmd.Flags().GetBool("use_etcd_operator")
	useEtcdOperatorSet := cmd.Flags().Changed("use_etcd_operator")
	customLabels, _ := cmd.Flags().GetString("labels")
	customAnnotations, _ := cmd.Flags().GetString("annotations")
	pemMemoryLimit, _ := cmd.Flags().GetString("pem_memory_limit")

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
	if customAnnotations != "" {
		_, err := k8s.KeyValueStringToMap(customAnnotations)
		if err != nil {
			utils.WithError(err).Fatal("--annotations must be specified through the following format: annotation1=value1,annotation2=value2")
		}
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
	inputVersionStr := versionString
	if len(versionString) == 0 {
		// Fetch latest version.
		versionString, err = getLatestVizierVersion(cloudConn)
		if err != nil {
			// Using log.Fatal rather than CLI log in order to track this unexpected error in Sentry.
			log.WithError(err).Fatal("Failed to fetch Vizier versions")
		}
	}
	utils.Infof("Installing version: %s", versionString)

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

	creds := auth.MustLoadDefaultCredentials()

	utils.Infof("Generating YAMLs for Pixie")

	var templatedYAMLs []*yamlsutils.YAMLFile
	templatedYAMLs, err = artifacts.FetchVizierTemplates(cloudConn, creds.Token, versionString)
	if err != nil {
		log.WithError(err).Fatal("Could not fetch Vizier YAMLs")
	}

	// useEtcdOperator is true then deploy operator. Otherwise: If defaultStorageExists, then
	// go for persistent storage version.
	if !useEtcdOperatorSet {
		// Validate correct number of default storage classes.
		defaultStorageExists, err := validateNumDefaultStorageClasses(clientset)
		if err != nil {
			utils.Error("Error checking default storage classes: " + err.Error())
		}
		if !defaultStorageExists {
			useEtcdOperator = true
		}
	}

	// Fill in template values.
	tmplValues := &vizieryamls.VizierTmplValues{
		DeployKey:         deployKey,
		CustomAnnotations: customAnnotations,
		CustomLabels:      customLabels,
		UseEtcdOperator:   useEtcdOperator,
		BootstrapVersion:  inputVersionStr,
		PEMMemoryLimit:    pemMemoryLimit,
		Namespace:         namespace,
	}

	clusterName, _ := cmd.Flags().GetString("cluster_name")
	setTemplateConfigValues(kubeAPIConfig.CurrentContext, tmplValues, cloudAddr, devCloudNS, clusterName)

	yamls, err := yamlsutils.ExecuteTemplatedYAMLs(templatedYAMLs, vizieryamls.VizierTmplValuesToArgs(tmplValues))
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

	od := k8s.ObjectDeleter{
		Namespace:  namespace,
		Clientset:  clientset,
		RestConfig: kubeConfig,
		Timeout:    2 * time.Minute,
	}
	// Get the number of nodes.
	numNodes, err := getNumNodes(clientset)
	if err != nil {
		utils.Error(err.Error())
	}

	utils.Infof("Found %v nodes", numNodes)

	vzYaml := yamlMap["vizier_persistent"]
	if useEtcdOperator {
		vzYaml = yamlMap["vizier_etcd"]
	}

	namespaceJob := newTaskWrapper("Creating namespace", func() error {
		// Create namespace, if needed.
		ns := &v1.Namespace{}
		ns.SetGroupVersionKind(v1.SchemeGroupVersion.WithKind("Namespace"))
		ns.Name = namespace

		_, err = clientset.CoreV1().Namespaces().Create(context.Background(), ns, metav1.CreateOptions{})
		if err != nil && k8serrors.IsAlreadyExists(err) {
			return nil
		}
		return err
	})

	clusterRoleJob := newTaskWrapper("Deleting stale Pixie objects, if any", func() error {
		_, err := od.DeleteByLabel("component=vizier", k8s.AllResourceKinds...)
		return err
	})

	certJob := newTaskWrapper("Deploying secrets and configmaps", func() error {
		err = k8s.ApplyYAML(clientset, kubeConfig, namespace, strings.NewReader(yamlMap["secrets"]), false)
		if err != nil {
			return err
		}

		// Launch roles, service accounts, and clusterroles, as the dependencies need the certs ready first.
		return k8s.ApplyYAMLForResourceTypes(clientset, kubeConfig, namespace, strings.NewReader(vzYaml),
			[]string{"podsecuritypolicies", "serviceaccounts", "clusterroles", "clusterrolebindings", "roles", "rolebindings", "jobs"}, false)
	})

	natsJob := newTaskWrapper("Deploying dependencies: NATS", func() error {
		return retryDeploy(clientset, kubeConfig, namespace, yamlMap["nats"])
	})

	etcdJob := newTaskWrapper("Deploying dependencies: etcd", func() error {
		return retryDeploy(clientset, kubeConfig, namespace, yamlMap["etcd"])
	})

	setupJobs := []utils.Task{
		namespaceJob, clusterRoleJob, certJob, natsJob}
	if useEtcdOperator {
		setupJobs = append(setupJobs, etcdJob)
	}

	jr := utils.NewSerialTaskRunner(setupJobs)
	err = jr.RunAndMonitor()
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

	clusterID := deploy(cloudConn, versionString, clientset, kubeConfig, vzYaml, namespace)

	waitForHealthCheck(cloudAddr, clusterID, clientset, namespace, numNodes)
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

	resp, err := v.ExecuteScriptStream(ctx, execScript)
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
				return errors.New("timeout waiting for healthcheck")
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

func deploy(cloudConn *grpc.ClientConn, version string, clientset *kubernetes.Clientset, config *rest.Config, yamlContents string, namespace string) uuid.UUID {
	var clusterID uuid.UUID
	deployJob := []utils.Task{
		newTaskWrapper("Deploying Cloud Connector", func() error {
			return k8s.ApplyYAML(clientset, config, namespace, strings.NewReader(yamlContents), false)
		}),
		newTaskWrapper("Waiting for Cloud Connector to come online", func() error {
			ctx, cancel := context.WithTimeout(context.Background(), 2*time.Minute)
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
					if cID, ok := s.Data["cluster-id"]; ok {
						clusterID = uuid.FromStringOrNil(string(cID))
						clusterIDExists = true
					}
				}
			}

			return waitForCluster(ctx, cloudConn, clusterID)
		}),
	}

	vzJr := utils.NewSerialTaskRunner(deployJob)
	err := vzJr.RunAndMonitor()
	if err != nil {
		// Using log.Fatal rather than CLI log in order to track this unexpected error in Sentry.
		log.WithError(err).Fatal("Failed to deploy Vizier")
	}
	return clusterID
}

func retryDeploy(clientset *kubernetes.Clientset, config *rest.Config, namespace string, yamlContents string) error {
	tries := 12
	var err error
	for tries > 0 {
		err = k8s.ApplyYAML(clientset, config, namespace, strings.NewReader(yamlContents), false)
		if err == nil {
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

func pemCanScheduleWithTaint(t *v1.Taint) bool {
	// For now an effect of NoSchedule should be sufficient, we don't have tolerations in the Daemonset spec.
	return t.Effect != "NoSchedule"
}

// validateNumDefaultStorageClasses returns a boolean whether there is exactly
// 1 default storage class or not.
func validateNumDefaultStorageClasses(clientset *kubernetes.Clientset) (bool, error) {
	storageClasses, err := k8s.ListStorageClasses(clientset)
	if err != nil {
		return false, err
	}

	defaultClassCount := 0

	// Check annotations map on each storage class to see if default is set to "true".
	for _, storageClass := range storageClasses.Items {
		annotationsMap := storageClass.GetAnnotations()
		if annotationsMap[DefaultClassAnnotationKey] == "true" {
			defaultClassCount++
		}
	}

	return defaultClassCount == 1, nil
}

func getNumNodes(clientset *kubernetes.Clientset) (int, error) {
	nodes, err := k8s.ListNodes(clientset)
	if err != nil {
		return 0, err
	}
	unscheduleableNodes := 0
	for _, n := range nodes.Items {
		for _, t := range n.Spec.Taints {
			if !pemCanScheduleWithTaint(&t) {
				unscheduleableNodes++
				break
			}
		}
	}
	return len(nodes.Items) - unscheduleableNodes, nil
}

var empty struct{}

// waitForPems waits for the Vizier's Proxy service to be ready with an external IP.
func waitForPems(clientset *kubernetes.Clientset, namespace string, expectedPods int) error {
	// Watch for pod updates.
	watcher, err := k8s.WatchK8sResource(clientset, "pods", namespace)
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
