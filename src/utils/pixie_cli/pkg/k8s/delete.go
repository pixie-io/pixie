package k8s

import (
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/client-go/kubernetes"
)

// DeleteNamespace deletes the given namespace.
func DeleteNamespace(clientset *kubernetes.Clientset, name string) error {
	namespaces := clientset.CoreV1().Namespaces()

	err := namespaces.Delete(name, &metav1.DeleteOptions{})
	if err != nil {
		return err
	}

	return nil
}

// DeleteClusterRole deletes the clusterrole with the given name.
func DeleteClusterRole(clientset *kubernetes.Clientset, name string) error {
	crs := clientset.RbacV1().ClusterRoles()
	err := crs.Delete(name, &metav1.DeleteOptions{})
	if err != nil {
		return err
	}

	return nil
}

// DeleteClusterRoleBinding deletes the clusterrolebinding with the given name.
func DeleteClusterRoleBinding(clientset *kubernetes.Clientset, name string) error {
	crbs := clientset.RbacV1().ClusterRoleBindings()

	err := crbs.Delete(name, &metav1.DeleteOptions{})
	if err != nil {
		return err
	}

	return nil
}

// DeleteConfigMap deletes the config map in the namespace with the given name.
func DeleteConfigMap(clientset *kubernetes.Clientset, name string, namespace string) error {
	cm := clientset.CoreV1().ConfigMaps(namespace)

	err := cm.Delete(name, &metav1.DeleteOptions{})
	if err != nil {
		return err
	}

	return nil
}

// DeleteAllResources deletes all resources in the given namespace with the given selector.
func DeleteAllResources(clientset *kubernetes.Clientset, ns string, selectors string) error {
	err := DeleteDeployments(clientset, ns, selectors)
	if err != nil {
		return err
	}

	err = DeleteDaemonSets(clientset, ns, selectors)
	if err != nil {
		return err
	}

	err = DeleteServices(clientset, ns, selectors)
	if err != nil {
		return err
	}

	err = DeletePods(clientset, ns, selectors)
	if err != nil {
		return err
	}

	return nil
}

// DeleteDeployments deletes all deployments in the namespace with the given selector.
func DeleteDeployments(clientset *kubernetes.Clientset, namespace string, selectors string) error {
	deployments := clientset.AppsV1().Deployments(namespace)

	if err := deployments.DeleteCollection(&metav1.DeleteOptions{}, metav1.ListOptions{LabelSelector: selectors}); err != nil {
		return err
	}
	return nil
}

// DeleteDaemonSets deletes all daemonsets in the namespace with the given selector.
func DeleteDaemonSets(clientset *kubernetes.Clientset, namespace string, selectors string) error {
	daemonsets := clientset.AppsV1().DaemonSets(namespace)

	if err := daemonsets.DeleteCollection(&metav1.DeleteOptions{}, metav1.ListOptions{LabelSelector: selectors}); err != nil {
		return err
	}
	return nil
}

// DeleteServices deletes all services in the namespace with the given selector.
func DeleteServices(clientset *kubernetes.Clientset, namespace string, selectors string) error {
	svcs := clientset.CoreV1().Services(namespace)

	l, err := svcs.List(metav1.ListOptions{LabelSelector: selectors})
	if err != nil {
		return err
	}
	for _, s := range l.Items {
		err = svcs.Delete(s.ObjectMeta.Name, &metav1.DeleteOptions{})
		if err != nil {
			return err
		}
	}

	return nil
}

// DeletePods deletes all pods in the namespace with the given selector.
func DeletePods(clientset *kubernetes.Clientset, namespace string, selectors string) error {
	pods := clientset.CoreV1().Pods(namespace)

	l, err := pods.List(metav1.ListOptions{LabelSelector: selectors})
	if err != nil {
		return err
	}
	for _, s := range l.Items {
		err = pods.Delete(s.ObjectMeta.Name, &metav1.DeleteOptions{})
		if err != nil {
			return err
		}
	}

	return nil
}
