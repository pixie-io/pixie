package cmd

import (
	"bufio"
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

	"google.golang.org/grpc/metadata"
	"pixielabs.ai/pixielabs/src/utils/pixie_cli/pkg/auth"

	"google.golang.org/grpc"
	"pixielabs.ai/pixielabs/src/cloud/cloudapipb"
	"pixielabs.ai/pixielabs/src/shared/services"

	log "github.com/sirupsen/logrus"
	"github.com/spf13/cobra"
	"github.com/spf13/viper"
	v1 "k8s.io/api/core/v1"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/client-go/kubernetes"

	"pixielabs.ai/pixielabs/src/utils/pixie_cli/pkg/certs"
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

// DeployCmd is the "deploy" command.
var DeployCmd = &cobra.Command{
	Use:   "deploy",
	Short: "Deploys Pixie on the current K8s cluster",
	Run:   runDeployCmd,
}

func init() {
	DeployCmd.Flags().StringP("extract_yaml", "e", "", "Directory to extract the Pixie yamls to")
	viper.BindPFlag("extract_yaml", DeployCmd.Flags().Lookup("extract_yaml"))

	DeployCmd.Flags().StringP("use_version", "v", "", "Pixie version to deploy")
	viper.BindPFlag("use_version", DeployCmd.Flags().Lookup("use_version"))

	DeployCmd.Flags().BoolP("check", "c", false, "Check whether the cluster can run Pixie")
	viper.BindPFlag("check", DeployCmd.Flags().Lookup("check"))

	DeployCmd.Flags().StringP("registration_key", "k", "", "The registration key to use for this cluster")
	viper.BindPFlag("registration_key", DeployCmd.Flags().Lookup("registration_key"))

	DeployCmd.Flags().StringP("credentials_file", "f", "", "Location of the Pixie credentials file")
	viper.BindPFlag("credentials_file", DeployCmd.Flags().Lookup("credentials_file"))

	DeployCmd.Flags().StringP("secret_name", "s", "pl-image-secret", "The name of the secret used to access the Pixie images")
	viper.BindPFlag("credentials_file", DeployCmd.Flags().Lookup("credentials_file"))

	DeployCmd.Flags().StringP("namespace", "n", "pl", "The namespace to install K8s secrets to")
	viper.BindPFlag("namespace", DeployCmd.Flags().Lookup("namespace"))

	DeployCmd.Flags().StringP("cluster_id", "i", "", "The ID of the cluster")
	viper.BindPFlag("cluster_id", DeployCmd.Flags().Lookup("cluster_id"))

	DeployCmd.Flags().BoolP("deps_only", "d", false, "Deploy only the cluster dependencies, not the agents")
	viper.BindPFlag("deps_only", DeployCmd.Flags().Lookup("deps_only"))
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
		log.WithError(err).Fatal("Failed to get creds")
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
	if check {
		err := k8s.RunDefaultClusterChecks()
		if err != nil {
			log.Fatalln(err)
		}
		return
	}

	currentCluster := getCurrentCluster()
	log.Info(fmt.Sprintf("Deploying Pixie to the following cluster: %s", currentCluster))
	log.Info("Is the cluster correct? (y/n)")
	clusterOk := acceptUserInput()
	if !clusterOk {
		log.Info("Cluster is not correct. Aborting.")
		return
	}

	kubeConfig := k8s.GetConfig()
	clientset := k8s.GetClientset(kubeConfig)
	namespace, _ := cmd.Flags().GetString("namespace")
	credsFile, _ := cmd.Flags().GetString("credentials_file")
	cloudAddr, _ := cmd.Flags().GetString("cloud_addr")
	clusterID, _ := cmd.Flags().GetString("cluster_id")

	// Get grpc connection to cloud.
	cloudConn, err := getCloudClientConnection(cloudAddr)
	if err != nil {
		log.Fatalln(err)
	}

	var credsData string
	if credsFile == "" {
		credsData = mustGetImagePullSecret(cloudConn)
	} else {
		credsData = mustReadCredsFile(credsFile)
	}
	optionallyCreateNamespace(clientset, namespace)

	// Install certs.
	optionallyInstallCerts(clientset, namespace)

	secretName, _ := cmd.Flags().GetString("secret_name")
	k8s.CreateDockerConfigJSONSecret(clientset, namespace, secretName, credsData)

	LoadClusterSecrets(clientset, cloudAddr, clusterID, namespace)

	versionString, err := cmd.Flags().GetString("use_version")
	if err != nil {
		log.Fatal("Version string is invalid")
	}

	if len(versionString) == 0 {
		// Fetch latest version.
		versionString, err = getLatestVizierVersion(cloudConn)
		if err != nil {
			log.Fatal("Failed to get Vizier version")
		}
	}

	reader, err := downloadVizierYAMLs(cloudConn, versionString)
	if err != nil {
		log.WithError(err).Fatal("Could not download Vizier YAMLs")
	}
	defer reader.Close()

	extractPath, _ := cmd.Flags().GetString("extract_yaml")
	// If extract_path is specified, write out yamls to file.
	if extractPath != "" {
		err := writeToFile(extractPath, "yamls.tar", reader)
		if err != nil {
			log.WithError(err).Fatal("Could not extract yamls to file")
		}
		reader, err = os.OpenFile(path.Join(extractPath, "yamls.tar"), os.O_RDWR, 0755)
		if err != nil {
			log.WithError(err).Fatal("Could not read yaml file")
		}
		defer reader.Close()
	}

	yamlMap, err := utils.ReadTarFileFromReader(reader)
	if err != nil {
		log.WithError(err).Fatal("Could not get YAMLs from tar")
	}

	depsOnly, _ := cmd.Flags().GetBool("deps_only")

	deploy(yamlMap, depsOnly)

	waitForProxy(clientset, namespace)
}

func acceptUserInput() bool {
	if viper.GetBool("y") {
		return true
	}
	for true {
		reader := bufio.NewReader(os.Stdin)
		text, _ := reader.ReadString('\n')
		if text == "y\n" || text == "yes\n" {
			return true
		} else if text == "n\n" || text == "no\n" {
			return false
		}
		log.Info("Please enter (y/n)")
	}
	return false
}

func getCurrentCluster() string {
	kcmd := exec.Command("kubectl", "config", "current-context")
	var out bytes.Buffer
	kcmd.Stdout = &out
	err := kcmd.Run()

	if err != nil {
		log.WithError(err).Fatal("Error getting current kubernetes cluster")
	}
	return out.String()
}

func optionallyInstallCerts(clientset *kubernetes.Clientset, namespace string) {
	secret := k8s.GetSecret(clientset, namespace, "service-tls-certs")
	// Check if secrets already exist. If not, then create them.
	if secret == nil {
		certs.DefaultInstallCerts(namespace, clientset)
	}
}

func optionallyCreateNamespace(clientset *kubernetes.Clientset, namespace string) {
	_, err := clientset.CoreV1().Namespaces().Get(namespace, metav1.GetOptions{})
	if err == nil {
		return
	}
	_, err = clientset.CoreV1().Namespaces().Create(&v1.Namespace{ObjectMeta: metav1.ObjectMeta{Name: namespace}})
	if err != nil {
		log.WithError(err).Fatalf("Error creating namespace %s", namespace)
	}
	log.Infof("Created namespace %s", namespace)
}

func deploy(yamlMap map[string]string, depsOnly bool) {
	// NATS and etcd deploys depend on timing, so may sometimes fail. Include some retry behavior.
	// TODO(zasgar/michelle): This logic is flaky and we should make smarter to actually detect and wait
	// based on the message.
	log.Info("Deploying NATS")
	retryDeploy(yamlMap[natsYAMLPath])
	log.Info("Deploying etcd")
	retryDeploy(yamlMap[etcdYAMLPath])

	if depsOnly {
		return
	}

	log.Info("Deploying Vizier")
	deployYAML(yamlMap[vizierYAMLPath])
}

func deployYAML(yamlContents string) error {
	kcmd := exec.Command("kubectl", "apply", "-f", "-")
	stdin, err := kcmd.StdinPipe()
	if err != nil {
		return err
	}

	go func() {
		defer stdin.Close()
		io.WriteString(stdin, yamlContents)
	}()

	return kcmd.Run()
}

func retryDeploy(yamlContents string) {
	tries := 5
	var err error
	for tries > 0 {
		err = deployYAML(yamlContents)
		if err == nil {
			break
		}
		time.Sleep(5 * time.Second)
		tries--
	}
	if tries == 0 {
		log.WithError(err).Fatal(fmt.Sprintf("Could not deploy YAML: %s", yamlContents))
	}
}

// waitForProxy waits for the Vizier's Proxy service to be ready with an external IP.
func waitForProxy(clientset *kubernetes.Clientset, namespace string) {
	log.Info("Waiting for services and pods to start...")

	// Watch for service updates.
	watcher, err := k8s.WatchK8sResource(clientset, "services", namespace)
	if err != nil {
		log.WithError(err).Fatal("Could not watch k8s services")
	}
	for c := range watcher.ResultChan() {
		service := c.Object.(*v1.Service)
		if service.ObjectMeta.Name == "vizier-proxy-service" {
			switch service.Spec.Type {
			case v1.ServiceTypeNodePort:
				{
					// TODO(zasgar): NodePorts get ready right away, we need to make sure
					// that the service is actually healthy.
					log.Info("Setup complete.")
					watcher.Stop()
				}
			case v1.ServiceTypeLoadBalancer:
				{
					if len(service.Status.LoadBalancer.Ingress) > 0 && service.Status.LoadBalancer.Ingress[0].IP != "" {
						log.Info("Setup complete.")
						watcher.Stop()
					}
				}
			}
		}
	}
}
