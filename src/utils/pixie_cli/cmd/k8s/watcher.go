package k8s

import (
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
