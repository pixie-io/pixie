package k8s

import (
	"context"

	v1 "k8s.io/api/core/v1"
	storagev1 "k8s.io/api/storage/v1"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/apimachinery/pkg/fields"
	"k8s.io/apimachinery/pkg/watch"
	"k8s.io/client-go/kubernetes"
	"k8s.io/client-go/tools/cache"
)

// WatchK8sResource returns a k8s watcher for the specified resource.
func WatchK8sResource(clientset *kubernetes.Clientset, resource string, namespace string) (watch.Interface, error) {
	watcher := cache.NewListWatchFromClient(clientset.CoreV1().RESTClient(), resource, namespace, fields.Everything())
	opts := metav1.ListOptions{}
	watch, err := watcher.Watch(opts)
	if err != nil {
		return nil, err
	}
	return watch, nil
}

// ListNodes lists the nodes in this Kubernetes cluster.
func ListNodes(clientset *kubernetes.Clientset) (*v1.NodeList, error) {
	opts := metav1.ListOptions{}
	return clientset.CoreV1().Nodes().List(context.Background(), opts)
}

// ListStorageClasses lists the storage classes in this Kubernetes cluster.
func ListStorageClasses(clientset *kubernetes.Clientset) (*storagev1.StorageClassList, error) {
	opts := metav1.ListOptions{}
	return clientset.StorageV1().StorageClasses().List(context.Background(), opts)
}
