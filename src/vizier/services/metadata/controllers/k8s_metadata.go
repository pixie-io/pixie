package controllers

import (
	log "github.com/sirupsen/logrus"
	v1 "k8s.io/api/core/v1"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/apimachinery/pkg/fields"
	"k8s.io/apimachinery/pkg/runtime"
	"k8s.io/client-go/kubernetes"
	// Blank import necessary for kubeConfig to work.
	_ "k8s.io/client-go/plugin/pkg/client/auth/gcp"
	"k8s.io/client-go/rest"
	"k8s.io/client-go/tools/cache"
)

// K8sMetadataController listens to any metadata updates from the K8s API.
type K8sMetadataController struct {
	mdHandler *MetadataHandler
	clientset *kubernetes.Clientset
}

// NewK8sMetadataController creates a new K8sMetadataController.
func NewK8sMetadataController(mdh *MetadataHandler) (*K8sMetadataController, error) {
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

	mc := &K8sMetadataController{mdHandler: mdh, clientset: clientset}

	// Clean up current metadata store state.
	pods, err := mc.listObject("pods")
	if err != nil {
		log.Info("Could not list all pods")
	}
	mdh.SyncPodData(pods.(*v1.PodList))

	eps, err := mc.listObject("endpoints")
	if err != nil {
		log.Info("Could not list all endpoints")
	}
	mdh.SyncEndpointsData(eps.(*v1.EndpointsList))

	services, err := mc.listObject("services")
	if err != nil {
		log.Info("Could not list all services")
	}
	mdh.SyncServiceData(services.(*v1.ServiceList))

	// Start up Watchers.
	go mc.startWatcher("pods")
	go mc.startWatcher("endpoints")
	go mc.startWatcher("services")

	return mc, nil
}

func (mc *K8sMetadataController) listObject(resource string) (runtime.Object, error) {
	watcher := cache.NewListWatchFromClient(mc.clientset.CoreV1().RESTClient(), resource, v1.NamespaceAll, fields.Everything())
	opts := metav1.ListOptions{}
	return watcher.List(opts)
}

func (mc *K8sMetadataController) startWatcher(resource string) {
	// Start up watcher for the given resource.
	watcher := cache.NewListWatchFromClient(mc.clientset.CoreV1().RESTClient(), resource, v1.NamespaceAll, fields.Everything())
	opts := metav1.ListOptions{}
	watch, err := watcher.Watch(opts)
	if err != nil {
		log.WithError(err).Fatal("Could not start watcher for k8s resource: " + resource)
	}

	for c := range watch.ResultChan() {
		msg := &K8sMessage{
			Object:     c.Object,
			ObjectType: resource,
		}
		mc.mdHandler.GetChannel() <- msg
	}
}
