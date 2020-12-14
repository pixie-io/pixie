package k8s

import (
	"fmt"
	"os"
	"path/filepath"
	"strings"

	log "github.com/sirupsen/logrus"
	"github.com/spf13/pflag"
	"k8s.io/client-go/discovery"
	"k8s.io/client-go/kubernetes"

	// Blank import necessary for kubeConfig to work.
	_ "k8s.io/client-go/plugin/pkg/client/auth"
	"k8s.io/client-go/rest"
	"k8s.io/client-go/tools/clientcmd"
)

// Contents in this file are copied and modified from
// https://github.com/kubernetes/client-go/blob/master/examples/out-of-cluster-client-configuration/main.go

var kubeconfig *string

// fileExists checks if a file exists and is not a directory before we
// try using it to prevent further errors.
func fileExists(filename string) bool {
	info, err := os.Stat(filename)
	if os.IsNotExist(err) {
		return false
	}
	return !info.IsDir()
}

func init() {
	defaultKubeConfig := ""
	optionalStr := "(optional) "
	if k := os.Getenv("KUBECONFIG"); k != "" {
		for _, config := range strings.Split(k, ":") {
			if fileExists(config) {
				defaultKubeConfig = config
				break
			}
		}
		if defaultKubeConfig == "" {
			log.Fatalln("Failed to find valid config in KUBECONFIG env. Is it formatted correctly?")
		}
	} else if home := homeDir(); home != "" {
		defaultKubeConfig = filepath.Join(home, ".kube", "config")
	} else {
		optionalStr = ""
	}

	kubeconfig = pflag.String("kubeconfig", defaultKubeConfig, fmt.Sprintf("%sabsolute path to the kubeconfig file", optionalStr))
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
