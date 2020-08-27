package main

import (
	"fmt"
	"strings"
	"time"

	log "github.com/sirupsen/logrus"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"
	"google.golang.org/grpc"
	"k8s.io/apimachinery/pkg/api/errors"
	"k8s.io/client-go/kubernetes"
	"k8s.io/client-go/rest"
	"pixielabs.ai/pixielabs/src/shared/services"
	"pixielabs.ai/pixielabs/src/utils/pixie_cli/pkg/artifacts"
	"pixielabs.ai/pixielabs/src/utils/shared/k8s"
)

const (
	etcdYAMLPath            = "./yamls/vizier_deps/etcd_prod.yaml"
	etcdOperatorYAMLPath    = "./yamls/vizier_deps/etcd_operator_prod.yaml"
	vizierYAMLPath          = "./yamls/vizier/vizier_prod.yaml"
	vizierBootstrapYAMLPath = "./yamls/vizier/vizier_bootstrap_prod.yaml"
	natsYAMLPath            = "./yamls/vizier_deps/nats_prod.yaml"
	deleteTimeout           = 10 * time.Minute
)

func init() {
	pflag.String("cloud_token", "", "The token to use for cloud connection")
	pflag.String("namespace", "pl", "The namespace used by Pixie")
	pflag.String("vizier_version", "", "The version to install or upgrade to")
	pflag.String("update_cloud_addr", "", "The pixie cloud address to use.")
	pflag.Bool("etcd_operator_enabled", false, "Whether the etcd operator should be used instead of the statefulset")
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

	cloudAddr := viper.GetString("update_cloud_addr")
	if cloudAddr == "" {
		cloudAddr = viper.GetString("cloud_addr")
	}
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

	// Update update role first.
	err = k8s.ApplyYAMLForResourceTypes(clientset, kubeConfig, "pl", strings.NewReader(yamlMap[vizierBootstrapYAMLPath]), []string{"clusterroles"})
	if err != nil {
		log.WithError(err).Fatalf("Failed to install vizier bootstrap for updater roles")
	}

	// Delete everything but updater dependencies.
	_, err = od.DeleteByLabel("component=vizier,vizier-updater-dep!=true", k8s.AllResourceKinds...)
	if err != nil {
		if errors.IsTimeout(err) {
			log.WithError(err).Error("Old components taking longer to terminate than timeout")
		} else {
			log.WithError(err).Fatal("Failed to delete old components")
		}
	}

	etcdPath := etcdYAMLPath
	if viper.GetBool("etcd_operator_enabled") {
		etcdPath = etcdOperatorYAMLPath
	}

	// If in bootstrap mode, deploy NATS and etcd.
	if viper.GetBool("bootstrap_mode") {
		log.Info("Deploying NATS")

		err = retryDeploy(clientset, kubeConfig, "pl", yamlMap[natsYAMLPath])
		if err != nil {
			log.WithError(err).Fatalf("Failed to deploy NATS")
		}

		log.Info("Deploying etcd")
		err = retryDeploy(clientset, kubeConfig, "pl", yamlMap[etcdPath])
		if err != nil {
			log.WithError(err).Fatalf("Failed to deploy etcd")
		}
	}

	// Redeploy etcd.
	if viper.GetBool("redeploy_etcd") {
		log.Info("Redeploying etcd")

		if viper.GetBool("etcd_operator_enabled") {
			_, err = od.DeleteByLabel("app=pl-monitoring", "etcdclusters.etcd.database.coreos.com")
			if err != nil {
				log.WithError(err).Error("Failed to delete old etcd")
			}

		} else {
			_, err = od.DeleteByLabel("app=pl-monitoring", "StatefulSet")
			if err != nil {
				if errors.IsTimeout(err) {
					log.WithError(err).Error("Existing etcd taking longer to terminate than timeout")
				} else {
					log.WithError(err).Fatal("Could not delete existing etc")
				}
			}
			_, err = od.DeleteByLabel("app=pl-monitoring", "PersistentVolumeClaim")
			if err != nil {
				if errors.IsTimeout(err) {
					log.WithError(err).Error("Existing etcd pvc taking longer to terminate than timeout")
				} else {
					log.WithError(err).Fatal("Could not delete etcd pvc")
				}
			}
		}

		err = retryDeploy(clientset, kubeConfig, "pl", yamlMap[etcdPath])
		if err != nil {
			log.WithError(err).Fatalf("Failed to redeploy etcd")
		}
	}

	err = k8s.ApplyYAML(clientset, kubeConfig, "pl", strings.NewReader(yamlMap[vizierYAMLPath]))
	if err != nil {
		log.WithError(err).Fatalf("Failed to install vizier")
	}

	log.Info("Done with update/install!")
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
