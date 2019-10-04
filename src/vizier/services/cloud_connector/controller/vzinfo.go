package controllers

import (
	"errors"

	corev1 "k8s.io/api/core/v1"
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
	// TODO(michelle): Make the service name a flag that can be passed in.
	proxySvc, err := v.clientset.CoreV1().Services(plNamespace).Get("vizier-proxy-service", metav1.GetOptions{})
	if err != nil {
		return "", err
	}

	ip := ""
	port := int32(0)

	if proxySvc.Spec.Type == corev1.ServiceTypeNodePort {
		nodesList, err := v.clientset.CoreV1().Nodes().List(metav1.ListOptions{})
		if err != nil {
			return "", err
		}
		if len(nodesList.Items) == 0 {
			return "", errors.New("Could not find node for NodePort")
		}

		// Just select the first node for now.
		// Don't want to randomly pick, because it'll affect DNS.
		nodeAddrs := nodesList.Items[0].Status.Addresses

		for _, nodeAddr := range nodeAddrs {
			if nodeAddr.Type == corev1.NodeInternalIP {
				ip = nodeAddr.Address
			}
		}
		if ip == "" {
			return "", errors.New("Could not determine IP address of node for NodePort")
		}

		for _, portSpec := range proxySvc.Spec.Ports {
			if portSpec.Name == "tcp-https" {
				port = portSpec.NodePort
			}
		}
		if port <= 0 {
			return "", errors.New("Could not determine port for vizier service")
		}
	} else if proxySvc.Spec.Type == corev1.ServiceTypeLoadBalancer {
		// It's possible to have more than one external IP. Just select the first one for now.
		if len(proxySvc.Status.LoadBalancer.Ingress) == 0 {
			return "", errors.New("Proxy service has no external IPs")
		}
		ip = proxySvc.Status.LoadBalancer.Ingress[0].IP

		for _, portSpec := range proxySvc.Spec.Ports {
			if portSpec.Name == "tcp-https" {
				port = portSpec.Port
			}
		}
		if port <= 0 {
			return "", errors.New("Could not determine port for vizier service")
		}
	} else {
		return "", errors.New("Unexpected service type")
	}

	externalAddr := ip

	return externalAddr, nil
}
