package controllers

import (
	log "github.com/sirupsen/logrus"
	v1 "k8s.io/api/core/v1"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/apimachinery/pkg/fields"
	"k8s.io/client-go/kubernetes"
	// Blank import necessary for kubeConfig to work.
	_ "k8s.io/client-go/plugin/pkg/client/auth/gcp"
	"k8s.io/client-go/tools/cache"
	"k8s.io/client-go/tools/clientcmd"
)

// K8sMetadataController listens to any metadata updates from the K8s API.
type K8sMetadataController struct {
	events chan *K8sMessage
}

// NewK8sMetadataController creates a new K8sMetadataController.
func NewK8sMetadataController(kubeConfigPath string, c chan *K8sMessage) (*K8sMetadataController, error) {
	kubeConfig, err := clientcmd.BuildConfigFromFlags("", kubeConfigPath)
	if err != nil {
		return nil, err
	}
	// Create k8s client.
	clientset, err := kubernetes.NewForConfig(kubeConfig)
	if err != nil {
		return nil, err
	}

	mc := &K8sMetadataController{events: c}

	// Start up Watchers.
	// TODO(michelle) : Add when D881 lands.
	// go mc.startWatcher(clientset.CoreV1().RESTClient(), "pods")
	go mc.startWatcher(clientset.CoreV1().RESTClient(), "endpoints")

	return mc, nil
}

func (mc *K8sMetadataController) startWatcher(client cache.Getter, resource string) {
	// Start up watcher for the given resource.
	watcher := cache.NewListWatchFromClient(client, resource, v1.NamespaceAll, fields.Everything())
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
		mc.events <- msg
	}
}
