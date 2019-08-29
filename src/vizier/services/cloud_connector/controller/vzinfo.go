package controllers

import (
	"errors"
	"fmt"

	"github.com/spf13/viper"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/client-go/kubernetes"

	// Blank import necessary for kubeConfig to work.
	_ "k8s.io/client-go/plugin/pkg/client/auth/gcp"
	"k8s.io/client-go/rest"
)

// TODO(michelle): Make namespace a flag that can be passed in.
const plNamespace = "pl"

// K8sVizierInfo is responsible for fetching Vizier information through K8s.
type K8sVizierInfo struct {
	clientset *kubernetes.Clientset
}

// NewK8sVizierInfo creates a new K8sVizierInfo.
func NewK8sVizierInfo() (*K8sVizierInfo, error) {
	// There is a specific config for services running in the cluster.
	kubeConfig, err := rest.InClusterConfig()
	if err != nil {
		return nil, err
	}

	// Create k8s client.
	clientset, err := kubernetes.NewForConfig(kubeConfig)
	if err != nil {
		return nil, err
	}

	return &K8sVizierInfo{
		clientset: clientset,
	}, nil
}

// GetAddress gets the external address of Vizier's proxy service.
func (v *K8sVizierInfo) GetAddress() (string, error) {
	protocol := "https"
	if viper.GetBool("disable_ssl") {
		protocol = "http"
	}

	// TODO(michelle): Make the service name a flag that can be passed in.
	proxySvc, err := v.clientset.CoreV1().Services(plNamespace).Get("vizier-proxy-service", metav1.GetOptions{})
	if err != nil {
		return "", err
	}

	// It's possible to have more than one external IP. Just select the first one for now.
	if len(proxySvc.Status.LoadBalancer.Ingress) == 0 {
		return "", errors.New("Proxy service has no external IPs")
	}
	ip := proxySvc.Status.LoadBalancer.Ingress[0].IP

	port := int32(0)
	for _, portSpec := range proxySvc.Spec.Ports {
		if portSpec.Name == "tcp-https" {
			port = portSpec.Port
		}
	}
	if port <= 0 {
		return "", errors.New("could not determine port for vizier service")
	}

	externalAddr := fmt.Sprintf("%s://%s:%d", protocol, ip, port)

	return externalAddr, nil
}
