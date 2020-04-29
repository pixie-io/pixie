package main

import (
	"fmt"
	"strings"
	"time"

	log "github.com/sirupsen/logrus"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"
	"google.golang.org/grpc"
	"k8s.io/client-go/kubernetes"
	"k8s.io/client-go/rest"
	"pixielabs.ai/pixielabs/src/shared/services"
	"pixielabs.ai/pixielabs/src/utils/pixie_cli/pkg/artifacts"
	"pixielabs.ai/pixielabs/src/utils/pixie_cli/pkg/k8s"
)

const (
	vizierYAMLPath = "./yamls/vizier/vizier_prod.yaml"
	deleteTimeout  = 5 * time.Minute
)

func init() {
	pflag.String("cloud_token", "", "The token to use for cloud connection")
	pflag.String("namespace", "pl", "The namespace used by Pixie")
	pflag.String("vizier_version", "", "The version to install or upgrade to")
	pflag.String("cloud_addr", "withpixie.ai:443", "The pixie cloud address to use.")
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

func main() {
	pflag.Parse()

	// Must call after all flags are setup.
	viper.AutomaticEnv()
	viper.SetEnvPrefix("PL")
	viper.BindPFlags(pflag.CommandLine)

	// Delete existing vizier if any, including cloud connector.
	// Install everything. Only messages after bootstrap components start will be sent.
	kubeConfig, err := rest.InClusterConfig()
	if err != nil {
		log.WithError(err).Fatal("Failed to create in cluster config")
	}
	// Create k8s client.
	clientset, err := kubernetes.NewForConfig(kubeConfig)
	if err != nil {
		log.WithError(err).Fatal("Failed to create in cluster client set")
	}

	cloudAddr := viper.GetString("cloud_addr")
	fmt.Printf("Using CLOUD: %s\n", cloudAddr)
	conn, err := getCloudClientConnection(cloudAddr)
	if err != nil {
		log.WithError(err).Fatal("Failed to get cloud connection")
	}

	version := viper.GetString("vizier_version")
	log.WithField("Version", version).Info("Fetching YAMLs")

	token := viper.GetString("cloud_token")
	yamlMap, err := artifacts.FetchVizierYAMLMap(conn, token, version)
	if err != nil {
		log.WithError(err).Fatal("Failed to download YAMLs")
	}

	ns := viper.GetString("namespace")
	od := k8s.ObjectDeleter{
		Namespace:  ns,
		Clientset:  clientset,
		RestConfig: kubeConfig,
		Timeout:    deleteTimeout,
	}
	// Delete everything but updater dependencies.
	err = od.DeleteByLabel("component=vizier,vizier-updater-dep!=true", k8s.AllResourceKinds...)
	if err != nil {
		log.WithError(err).Fatal("Failed to delete old components")
	}

	err = k8s.ApplyYAML(clientset, kubeConfig, "pl", strings.NewReader(yamlMap[vizierYAMLPath]))
	if err != nil {
		log.WithError(err).Fatalf("Failed to install vizier")
	}

	log.Info("Done with update/install!")
}
