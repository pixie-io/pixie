package controllers

import (
	"sync"
	"time"

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
)

// K8sMetadataController listens to any metadata updates from the K8s API and forwards them
// to a channel where it can be processed.
type K8sMetadataController struct {
	quitCh   chan struct{}
	updateCh chan *K8sResourceMessage
	once     sync.Once
	watchers []MetadataWatcher
}

// MetadataWatcher watches a k8s resource type and forwards the updates to the given update channel.
type MetadataWatcher interface {
	Sync(storedUpdates []*storepb.K8SResource) error
	StartWatcher(chan struct{}, *sync.WaitGroup)
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
	// The resource types we watch the K8s API for. These types are in a specific order:
	// for example, nodes and namespaces must be synced before pods, since nodes/namespaces
	// contain pods.
	watchers := []MetadataWatcher{
		NewNodeWatcher("nodes", updateCh, clientset),
		NewNamespaceWatcher("namespaces", updateCh, clientset),
		NewPodWatcher("pods", updateCh, clientset),
		NewEndpointsWatcher("endpoints", updateCh, clientset),
		NewServiceWatcher("services", updateCh, clientset),
	}

	mc := &K8sMetadataController{quitCh: quitCh, updateCh: updateCh, watchers: watchers}

	go mc.Start(mds)

	return mc, nil
}

// Start starts the k8s watcher. Every 12h, it will resync such that the updates from the
// last 24h will always contain updates from currently running resources.
func (mc *K8sMetadataController) Start(mds K8sMetadataStore) error {
	// Run initial sync and watch.
	watcherQuitCh := make(chan struct{})
	var wg sync.WaitGroup

	err := mc.syncAndWatch(mds, watcherQuitCh, &wg)
	if err != nil {
		return err
	}

	ticker := time.NewTicker(12 * time.Hour)

	defer func() {
		close(watcherQuitCh)
		ticker.Stop()
	}()

	for {
		select {
		case <-ticker.C:
			close(watcherQuitCh) // Stop previous watcher, resync to send updates for all currently running resources.
			watcherQuitCh = make(chan struct{})
			wg.Wait()
			err = mc.syncAndWatch(mds, watcherQuitCh, &wg)
			if err != nil {
				return err
			}
		case <-mc.quitCh:
			return nil
		}
	}
}

// Start syncs the state stored in the datastore with what is currently running in k8s.
func (mc *K8sMetadataController) syncAndWatch(mds K8sMetadataStore, quitCh chan struct{}, wg *sync.WaitGroup) error {
	storedUpdates, err := mds.FetchFullResourceUpdates(0, 0)
	if err != nil {
		return err
	}
	for _, w := range mc.watchers {
		err := w.Sync(storedUpdates)
		if err != nil {
			return err
		}
		wg.Add(1)
		go w.StartWatcher(quitCh, wg)
	}

	return nil
}

// Stop stops all K8s watchers.
func (mc *K8sMetadataController) Stop() {
	mc.once.Do(func() {
		close(mc.quitCh)
	})
}
