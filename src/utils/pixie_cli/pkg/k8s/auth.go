package k8s

import (
	"os"
	"path/filepath"

	log "github.com/sirupsen/logrus"
	"github.com/spf13/pflag"
	"k8s.io/client-go/discovery"
	"k8s.io/client-go/kubernetes"

	// Blank import necessary for kubeConfig to work.
	_ "k8s.io/client-go/plugin/pkg/client/auth/gcp"
	"k8s.io/client-go/rest"
	"k8s.io/client-go/tools/clientcmd"
)

// Contents in this file are copied and modified from
// https://github.com/kubernetes/client-go/blob/master/examples/out-of-cluster-client-configuration/main.go

var kubeconfig *string

func init() {
	if home := homeDir(); home != "" {
		kubeconfig = pflag.String("kubeconfig", filepath.Join(home, ".kube", "config"), "(optional) absolute path to the kubeconfig file")
	} else if k := os.Getenv("KUBECONFIG"); k != "" {
		kubeconfig = pflag.String("kubeconfig", k, "(optional) absolute path to the kubeconfig file")
	} else {
		kubeconfig = pflag.String("kubeconfig", "", "absolute path to the kubeconfig file")
	}
}

// GetClientset gets the clientset for the current kubernetes cluster.
func GetClientset(config *rest.Config) *kubernetes.Clientset {
	clientset, err := kubernetes.NewForConfig(config)
	if err != nil {
		log.WithError(err).Fatal("Could not create k8s clientset")
	}
	return clientset
}

// GetDiscoveryClient gets the discovery client for the current kubernetes cluster.
func GetDiscoveryClient(config *rest.Config) *discovery.DiscoveryClient {
	discoveryClient, err := discovery.NewDiscoveryClientForConfig(config)
	if err != nil {
		log.WithError(err).Fatal("Could not create k8s discovery client")
	}

	return discoveryClient
}

// GetConfig gets the kubernetes rest config.
func GetConfig() *rest.Config {
	// use the current context in kubeconfig
	config, err := clientcmd.BuildConfigFromFlags("", *kubeconfig)
	if err != nil {
		log.WithError(err).Fatal("Could not build kubeconfig")
	}

	return config
}

func homeDir() string {
	if h := os.Getenv("HOME"); h != "" {
		return h
	}
	return os.Getenv("USERPROFILE") // windows
}
