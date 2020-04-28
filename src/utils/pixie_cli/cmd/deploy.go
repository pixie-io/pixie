package cmd

import (
	"bytes"
	"context"
	"errors"
	"fmt"
	"io"
	"io/ioutil"
	"net/http"
	"os"
	"os/exec"
	"path"
	"strings"
	"time"

	"github.com/fatih/color"
	"google.golang.org/grpc/metadata"
	"gopkg.in/segmentio/analytics-go.v3"
	"pixielabs.ai/pixielabs/src/shared/version"
	"pixielabs.ai/pixielabs/src/utils/pixie_cli/pkg/script"

	"pixielabs.ai/pixielabs/src/utils/pixie_cli/pkg/auth"
	"pixielabs.ai/pixielabs/src/utils/pixie_cli/pkg/pxanalytics"
	"pixielabs.ai/pixielabs/src/utils/pixie_cli/pkg/pxconfig"

	"google.golang.org/grpc"
	"pixielabs.ai/pixielabs/src/cloud/cloudapipb"
	"pixielabs.ai/pixielabs/src/shared/services"

	log "github.com/sirupsen/logrus"
	"github.com/spf13/cobra"
	"github.com/spf13/viper"
	v1 "k8s.io/api/core/v1"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/client-go/kubernetes"
	"k8s.io/client-go/rest"

	"pixielabs.ai/pixielabs/src/utils/pixie_cli/pkg/certs"
	"pixielabs.ai/pixielabs/src/utils/pixie_cli/pkg/components"
	"pixielabs.ai/pixielabs/src/utils/pixie_cli/pkg/k8s"
	"pixielabs.ai/pixielabs/src/utils/pixie_cli/pkg/utils"
)

const (
	k8sMinVersion    = "1.8"
	kernelMinVersion = "4.14"
)

const (
	etcdYAMLPath   = "./yamls/vizier_deps/etcd_prod.yaml"
	natsYAMLPath   = "./yamls/vizier_deps/nats_prod.yaml"
	vizierYAMLPath = "./yamls/vizier/vizier_prod.yaml"
)

// Sentry configs are not actually secret and safe to check in.
const (
	// We can't really distinguish between prod/dev, so we use some heuristics to decide.
	prodSentryDSN = "https://a8a635734bb840799befb63190e904e0@o324879.ingest.sentry.io/5203506"
	devSentryDSN  = "https://8e4acf22871543f1aa143a93a5216a16@o324879.ingest.sentry.io/5203508"
)

func getSentryDSN(vizierVersion string) string {
	// Only we have dev CLI.
	if version.GetVersion().IsDev() {
		return devSentryDSN
	}
	// If it contains - it must be a pre-release Vizier.
	if strings.Contains(vizierVersion, "-") {
		return devSentryDSN
	}
	return prodSentryDSN
}

// DeployCmd is the "deploy" command.
var DeployCmd = &cobra.Command{
	Use:   "deploy",
	Short: "Deploys Pixie on the current K8s cluster",
	PreRun: func(cmd *cobra.Command, args []string) {
		if e, has := os.LookupEnv("PL_VIZIER_VERSION"); has {
			viper.Set("use_version", e)
		}
	},
	PostRun: func(cmd *cobra.Command, args []string) {
		p := func(s string, a ...interface{}) {
			fmt.Fprintf(os.Stderr, s, a...)
		}
		u := color.New(color.Underline).Sprintf
		b := color.New(color.Bold)
		g := color.GreenString

		fmt.Fprint(os.Stderr, "\n")
		p(color.CyanString("==> ") + b.Sprint("Next Steps:\n"))
		p("\nVisit : %s to use Pixie's UI.\n", u("https://work.withpixie.ai"))
		p("\nRun some scripts using the %s cli. For example: \n", g("px"))
		p("- %s : to show pre-installed scripts.\n", g("px script list"))
		p("- %s : to run service info for sock-shop demo application (service selection coming soon!).\n",
			g("px run %s", script.ServiceStatsScript))
		p("\nCheck out our docs: %s.\n", u("https://work.withpixie.ai/docs"))
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

	DeployCmd.Flags().StringP("use_version", "v", "", "Pixie version to deploy")
	viper.BindPFlag("use_version", DeployCmd.Flags().Lookup("use_version"))

	DeployCmd.Flags().BoolP("check", "c", true, "Check whether the cluster can run Pixie")
	viper.BindPFlag("check", DeployCmd.Flags().Lookup("check"))

	DeployCmd.Flags().BoolP("check_only", "", false, "Only run check and exit.")
	viper.BindPFlag("check_only", DeployCmd.Flags().Lookup("check_only"))

	DeployCmd.Flags().StringP("credentials_file", "f", "", "Location of the Pixie credentials file")
	viper.BindPFlag("credentials_file", DeployCmd.Flags().Lookup("credentials_file"))

	DeployCmd.Flags().StringP("secret_name", "s", "pl-image-secret", "The name of the secret used to access the Pixie images")
	viper.BindPFlag("credentials_file", DeployCmd.Flags().Lookup("credentials_file"))

	DeployCmd.Flags().StringP("namespace", "n", "pl", "The namespace to install K8s secrets to")
	viper.BindPFlag("namespace", DeployCmd.Flags().Lookup("namespace"))

	DeployCmd.Flags().BoolP("deps_only", "d", false, "Deploy only the cluster dependencies, not the agents")
	viper.BindPFlag("deps_only", DeployCmd.Flags().Lookup("deps_only"))

	DeployCmd.Flags().StringP("dev_cloud_namespace", "m", "", "The namespace of Pixie Cloud, if running Cloud on minikube")
	viper.BindPFlag("dev_cloud_namespace", DeployCmd.Flags().Lookup("dev_cloud_namespace"))

	// Super secret flags for Pixies.
	DeployCmd.Flags().MarkHidden("namespace")
	DeployCmd.Flags().MarkHidden("dev_cloud_namespace")
	DeployCmd.Flags().MarkHidden("cluster_id")
}

func newVizAuthClient(conn *grpc.ClientConn) cloudapipb.VizierImageAuthorizationClient {
	return cloudapipb.NewVizierImageAuthorizationClient(conn)
}

func newArtifactTrackerClient(conn *grpc.ClientConn) cloudapipb.ArtifactTrackerClient {
	return cloudapipb.NewArtifactTrackerClient(conn)
}

func getCloudClientConnection(cloudAddr string) (*grpc.ClientConn, error) {
	isInternal := strings.ContainsAny(cloudAddr, "cluster.local")

	dialOpts, err := services.GetGRPCClientDialOptsServerSideTLS(isInternal)
	if err != nil {
		return nil, err
	}

	c, err := grpc.Dial(cloudAddr, dialOpts...)
	if err != nil {
		return nil, err
	}

	return c, nil
}

func mustGetImagePullSecret(conn *grpc.ClientConn) string {
	// Make rpc request to the cloud to get creds.
	client := newVizAuthClient(conn)
	creds, err := auth.LoadDefaultCredentials()
	if err != nil {
		log.WithError(err).Fatal("Failed to get creds. You might have to run: 'pixie auth login'")
	}
	req := &cloudapipb.GetImageCredentialsRequest{}
	ctxWithCreds := metadata.AppendToOutgoingContext(context.Background(), "authorization",
		fmt.Sprintf("bearer %s", creds.Token))

	resp, err := client.GetImageCredentials(ctxWithCreds, req)
	if err != nil {
		log.WithError(err).Fatal("Failed to fetch image credentials")
	}
	return resp.Creds
}

func mustReadCredsFile(credsFile string) string {
	credsData, err := ioutil.ReadFile(credsFile)
	if err != nil {
		log.WithError(err).Fatal(fmt.Sprintf("Could not read file: %s", credsFile))
	}
	return string(credsData)
}

func downloadFile(url string) (io.ReadCloser, error) {
	// Get the data
	resp, err := http.Get(url)
	if err != nil {
		return nil, err
	}
	return resp.Body, nil
}

func downloadVizierYAMLs(conn *grpc.ClientConn, version string) (io.ReadCloser, error) {
	client := newArtifactTrackerClient(conn)

	creds, err := auth.LoadDefaultCredentials()
	if err != nil {
		return nil, err
	}

	req := &cloudapipb.GetDownloadLinkRequest{
		ArtifactName: "vizier",
		VersionStr:   version,
		ArtifactType: cloudapipb.AT_CONTAINER_SET_YAMLS,
	}
	ctxWithCreds := metadata.AppendToOutgoingContext(context.Background(), "authorization",
		fmt.Sprintf("bearer %s", creds.Token))

	resp, err := client.GetDownloadLink(ctxWithCreds, req)
	if err != nil {
		return nil, err
	}

	return downloadFile(resp.Url)
}

func writeToFile(filepath string, filename string, reader io.ReadCloser) error {
	// Create directory for the files.
	if _, err := os.Stat(filepath); os.IsNotExist(err) {
		os.Mkdir(filepath, 0777)
	}

	out, err := os.Create(path.Join(filepath, filename))
	if err != nil {
		return err
	}
	defer out.Close()

	// Write the body to file
	_, err = io.Copy(out, reader)

	reader.Close()
	return err
}

func getLatestVizierVersion(conn *grpc.ClientConn) (string, error) {
	client := newArtifactTrackerClient(conn)

	creds, err := auth.LoadDefaultCredentials()
	if err != nil {
		return "", err
	}

	req := &cloudapipb.GetArtifactListRequest{
		ArtifactName: "vizier",
		ArtifactType: cloudapipb.AT_CONTAINER_SET_YAMLS,
		Limit:        1,
	}
	ctxWithCreds := metadata.AppendToOutgoingContext(context.Background(), "authorization",
		fmt.Sprintf("bearer %s", creds.Token))

	resp, err := client.GetArtifactList(ctxWithCreds, req)
	if err != nil {
		return "", err
	}

	if len(resp.Artifact) != 1 {
		return "", errors.New("Could not find Vizier artifact")
	}

	return resp.Artifact[0].VersionStr, nil
}

func runDeployCmd(cmd *cobra.Command, args []string) {
	check, _ := cmd.Flags().GetBool("check")
	checkOnly, _ := cmd.Flags().GetBool("check_only")
	if check || checkOnly {
		_ = pxanalytics.Client().Enqueue(&analytics.Track{
			UserId: pxconfig.Cfg().UniqueClientID,
			Event:  "Cluster Check Run",
		})

		err := k8s.RunDefaultClusterChecks()
		if err != nil {
			_ = pxanalytics.Client().Enqueue(&analytics.Track{
				UserId: pxconfig.Cfg().UniqueClientID,
				Event:  "Cluster Check Failed",
				Properties: analytics.NewProperties().
					Set("error", err.Error()),
			})
			log.WithError(err).Fatalln("Check pre-check has failed. To by pass pass in --check=false.")
		}

		if checkOnly {
			fmt.Print("\nAll Checks Passed!\n")
			os.Exit(0)
		}
	}

	namespace, _ := cmd.Flags().GetString("namespace")
	credsFile, _ := cmd.Flags().GetString("credentials_file")
	devCloudNS := viper.GetString("dev_cloud_namespace")

	cloudAddr := viper.GetString("cloud_addr")

	_ = pxanalytics.Client().Enqueue(&analytics.Track{
		UserId: pxconfig.Cfg().UniqueClientID,
		Event:  "Deploy Initiated",
		Properties: analytics.NewProperties().
			Set("cloud_addr", cloudAddr),
	})

	status, clusterID, err := getClusterID(cloudAddr)
	if err != nil {
		log.Fatal(err)
	}

	fmt.Printf("Deploying to Pixie Cluster ID: %s.\n", clusterID.String())

	if *status != cloudapipb.CS_UNKNOWN && *status != cloudapipb.CS_DISCONNECTED {
		log.Fatal("Cluster must be disconnected to deploy, please delete the install if you want to re-deploy")
	}

	_ = pxanalytics.Client().Enqueue(&analytics.Track{
		UserId: pxconfig.Cfg().UniqueClientID,
		Event:  "Deploy Started",
		Properties: analytics.NewProperties().
			Set("clusterID", clusterID.String()).
			Set("cloud_addr", cloudAddr),
	})

	currentCluster := getCurrentCluster()
	fmt.Printf("Deploying Pixie to the following cluster: %s\n", currentCluster)
	clusterOk := components.YNPrompt("Is the cluster correct?", true)
	if !clusterOk {
		fmt.Printf("Cluster is not correct. Aborting.")
		return
	}

	kubeConfig := k8s.GetConfig()
	clientset := k8s.GetClientset(kubeConfig)
	// Get the number of nodes.
	numNodes, err := getNumNodes(clientset)
	if err != nil {
		fmt.Println(err.Error())
	}

	fmt.Printf("Found %v nodes\n", numNodes)

	// Get grpc connection to cloud.
	cloudConn, err := getCloudClientConnection(cloudAddr)
	if err != nil {
		log.Fatalln(err)
	}

	versionString := viper.GetString("use_version")
	if len(versionString) == 0 {
		// Fetch latest version.
		versionString, err = getLatestVizierVersion(cloudConn)
		if err != nil {
			log.WithError(err).Fatal("Failed to fetch Vizier versions")
		}
	}
	fmt.Printf("Installing version: %s\n", versionString)

	namespaceJob := newTaskWrapper("Creating namespace", func() error {
		return optionallyCreateNamespace(clientset, namespace)
	})

	certJob := newTaskWrapper("Installing certs", func() error {
		return optionallyInstallCerts(clientset, namespace)
	})

	secretJob := newTaskWrapper("Loading secrets", func() error {
		versionString := viper.GetString("use_version")
		var credsData string
		if credsFile == "" {
			credsData = mustGetImagePullSecret(cloudConn)
		} else {
			credsData = mustReadCredsFile(credsFile)
		}

		secretName, _ := cmd.Flags().GetString("secret_name")
		err := k8s.CreateDockerConfigJSONSecret(clientset, namespace, secretName, credsData)
		if err != nil {
			return err
		}
		return LoadClusterSecrets(clientset, cloudAddr, clusterID.String(), namespace, devCloudNS, kubeConfig, getSentryDSN(versionString))
	})

	clusterRoleJob := newTaskWrapper("Updating clusterroles", func() error {
		return optionallyDeleteClusterrole(clientset)
	})

	var yamlMap map[string]string
	yamlJob := newTaskWrapper("Downloading Vizier YAMLs", func() error {
		reader, err := downloadVizierYAMLs(cloudConn, versionString)
		if err != nil {
			return err
		}
		defer reader.Close()

		extractPath, _ := cmd.Flags().GetString("extract_yaml")
		// If extract_path is specified, write out yamls to file.
		if extractPath != "" {
			err := writeToFile(extractPath, "yamls.tar", reader)
			if err != nil {
				return err
			}
			reader, err = os.OpenFile(path.Join(extractPath, "yamls.tar"), os.O_RDWR, 0755)
			if err != nil {
				return err
			}
			defer reader.Close()
		}

		yamlMap, err = utils.ReadTarFileFromReader(reader)
		if err != nil {
			return err
		}
		return nil
	})

	setupJobs := []utils.Task{
		namespaceJob, certJob, secretJob, clusterRoleJob, yamlJob,
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
		log.Fatal("Failed to deploy Vizier")
	}

	depsOnly, _ := cmd.Flags().GetBool("deps_only")

	deploy(clientset, kubeConfig, yamlMap, namespace, depsOnly)
	waitForHealthCheck(cloudAddr, clientset, namespace, numNodes)
}

func waitForHealthCheck(cloudAddr string, clientset *kubernetes.Clientset, namespace string, numNodes int) {
	runSimpleScript := func() error {
		v, err := connectDefaultVizier(cloudAddr)
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

	fmt.Printf("Waiting for Pixie to pass healthcheck\n")

	healthCheckJobs := []utils.Task{
		newTaskWrapper("Wait for PEMs/Kelvin", func() error {
			return waitForPems(clientset, namespace, numNodes)
		}),
		newTaskWrapper("Wait for healthcheck", func() error {
			timeout := time.NewTimer(2 * time.Minute)
			defer timeout.Stop()
			for {
				select {
				case <-timeout.C:
					return errors.New("timeout waiting for healthcheck")
				default:
					err := runSimpleScript()
					if err == nil {
						return nil
					}
					time.Sleep(5 * time.Second)
				}
			}
		}),
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
		log.WithError(err).Fatal("Failed Pixie healthcheck")
	}
	_ = pxanalytics.Client().Enqueue(&analytics.Track{
		UserId: pxconfig.Cfg().UniqueClientID,
		Event:  "Deploy Healthcheck Passed",
	})
}
func getCurrentCluster() string {
	kcmd := exec.Command("kubectl", "config", "current-context")
	var out bytes.Buffer
	kcmd.Stdout = &out
	kcmd.Stderr = os.Stderr
	err := kcmd.Run()

	if err != nil {
		log.WithError(err).Fatal("Error getting current kubernetes cluster")
	}
	return out.String()
}

func optionallyInstallCerts(clientset *kubernetes.Clientset, namespace string) error {
	secret := k8s.GetSecret(clientset, namespace, "service-tls-certs")
	// Check if secrets already exist. If not, then create them.
	if secret == nil {
		certs.DefaultInstallCerts(namespace, clientset)
	}
	return nil
}

func optionallyCreateNamespace(clientset *kubernetes.Clientset, namespace string) error {
	_, err := clientset.CoreV1().Namespaces().Get(context.Background(), namespace, metav1.GetOptions{})
	if err == nil {
		return nil
	}
	_, err = clientset.CoreV1().Namespaces().Create(context.Background(), &v1.Namespace{ObjectMeta: metav1.ObjectMeta{Name: namespace}}, metav1.CreateOptions{})
	if err != nil {
		return err
	}
	return nil
}

func optionallyDeleteClusterrole(clientset *kubernetes.Clientset) error {
	_ = k8s.DeleteClusterRole(clientset, "vizier-metadata")
	_ = k8s.DeleteClusterRoleBinding(clientset, "vizier-metadata")
	return nil
}

func deploy(clientset *kubernetes.Clientset, config *rest.Config, yamlMap map[string]string, namespace string, depsOnly bool) {
	// NATS and etcd deploys depend on timing, so may sometimes fail. Include some retry behavior.
	// TODO(zasgar/michelle): This logic is flaky and we should make smarter to actually detect and wait
	// based on the message.
	natsJob := newTaskWrapper("Deploying NATS", func() error {
		return retryDeploy(clientset, config, namespace, yamlMap[natsYAMLPath])
	})

	etcdJob := newTaskWrapper("Deploying etcd", func() error {
		return retryDeploy(clientset, config, namespace, yamlMap[etcdYAMLPath])
	})

	deployDepsJobs := []utils.Task{natsJob, etcdJob}

	jr := utils.NewParallelTaskRunner(deployDepsJobs)
	err := jr.RunAndMonitor()
	if err != nil {
		log.Fatal("Failed to deploy Vizier deps")
	}

	if depsOnly {
		return
	}

	deployJob := []utils.Task{
		newTaskWrapper("Deploying Vizier", func() error {
			return k8s.ApplyYAML(clientset, config, namespace, strings.NewReader(yamlMap[vizierYAMLPath]))
		}),
	}

	vzJr := utils.NewSerialTaskRunner(deployJob)
	err = vzJr.RunAndMonitor()
	if err != nil {
		log.Fatal("Failed to deploy Vizier")
	}
}

func retryDeploy(clientset *kubernetes.Clientset, config *rest.Config, namespace string, yamlContents string) error {
	tries := 12
	var err error
	for tries > 0 {
		err = k8s.ApplyYAML(clientset, config, namespace, strings.NewReader(yamlContents))
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
	if t.Effect == "NoSchedule" {
		return false
	}
	return true
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
			// TODO(philkuz/zasgar) should we make this a print line instead?
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
