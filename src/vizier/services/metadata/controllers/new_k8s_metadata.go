package controllers

import (
	"sync"

	v1 "k8s.io/api/core/v1"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/apimachinery/pkg/fields"
	"k8s.io/apimachinery/pkg/runtime"
	"k8s.io/client-go/kubernetes"

	// Blank import necessary for kubeConfig to work.
	_ "k8s.io/client-go/plugin/pkg/client/auth/gcp"
	"k8s.io/client-go/rest"
	"k8s.io/client-go/tools/cache"

	storepb "pixielabs.ai/pixielabs/src/vizier/services/metadata/storepb"
)

var (
	kubeSystemNs       = "kube-system"
	kubeProxyPodPrefix = "kube-proxy"
	// The resource types we watch the K8s API for. These types are in a specific order:
	// for example, nodes and namespaces must be synced before pods, since nodes/namespaces
	// contain pods.
	resourceTypes = []string{"nodes", "namespaces", "pods", "endpoints", "services"}
)

// K8sMetadataController listens to any metadata updates from the K8s API and forwards them
// to a channel where it can be processed.
type K8sMetadataController struct {
	quitCh   chan struct{}
	updateCh chan *K8sResourceMessage
	once     sync.Once
	watchers map[string]MetadataWatcher
}

// MetadataWatcher watches a k8s resource type and forwards the updates to the given update channel.
type MetadataWatcher interface {
	Sync(storedUpdates []*storepb.K8SResource) error
	StartWatcher()
}

func listObject(resource string, clientset *kubernetes.Clientset) (runtime.Object, error) {
	watcher := cache.NewListWatchFromClient(clientset.CoreV1().RESTClient(), resource, v1.NamespaceAll, fields.Everything())
	opts := metav1.ListOptions{}
	return watcher.List(opts)
}

// NewK8sMetadataController creates a new K8sMetadataController.
func NewK8sMetadataController(mds K8sMetadataStore, updateCh chan *K8sResourceMessage) (*K8sMetadataController, error) {
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

	quitCh := make(chan struct{})

	// Create a watcher for each resource.
	watchers := make(map[string]MetadataWatcher)
	watchers["nodes"] = NewNodeWatcher("nodes", quitCh, updateCh, clientset)
	watchers["namespaces"] = NewNamespaceWatcher("namespaces", quitCh, updateCh, clientset)
	watchers["pods"] = NewPodWatcher("pods", quitCh, updateCh, clientset)
	watchers["endpoints"] = NewEndpointsWatcher("endpoints", quitCh, updateCh, clientset)
	watchers["services"] = NewServiceWatcher("services", quitCh, updateCh, clientset)

	mc := &K8sMetadataController{quitCh: quitCh, updateCh: updateCh, watchers: watchers}

	// Sync current state.
	err = mc.syncState(mds)
	if err != nil {
		return nil, err
	}

	// Start up Watchers.
	mc.startWatchers()

	return mc, nil
}

// SyncState syncs the state stored in the datastore with what is currently running in k8s.
func (mc *K8sMetadataController) syncState(mds K8sMetadataStore) error {
	storedUpdates, err := mds.FetchFullResourceUpdates(0, 0)
	if err != nil {
		return err
	}
	for _, r := range resourceTypes {
		err := mc.watchers[r].Sync(storedUpdates)
		if err != nil {
			return err
		}
	}
	return nil
}

// StartWatchers starts all of the resource watchers.
func (mc *K8sMetadataController) startWatchers() {
	for _, r := range resourceTypes {
		go mc.watchers[r].StartWatcher()
	}
}

// Stop stops all K8s watchers.
func (mc *K8sMetadataController) Stop() {
	mc.once.Do(func() {
		close(mc.quitCh)
	})
}
