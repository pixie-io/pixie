package k8s

import (
	"context"
	"encoding/json"
	"io"

	"github.com/sirupsen/logrus"
	"k8s.io/apimachinery/pkg/api/errors"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/apimachinery/pkg/apis/meta/v1/unstructured"
	"k8s.io/apimachinery/pkg/runtime"
	"k8s.io/apimachinery/pkg/runtime/schema"
	"k8s.io/apimachinery/pkg/util/yaml"
	"k8s.io/client-go/dynamic"
	"k8s.io/client-go/kubernetes"
	"k8s.io/client-go/rest"
	"k8s.io/client-go/restmapper"
)

// ApplyYAML does the equivalent of a kubectl apply for the given yaml.
// This function is copied from https://stackoverflow.com/a/47139247 with major updates with
// respect to API changes and some clean up work.
func ApplyYAML(clientset *kubernetes.Clientset, config *rest.Config, namespace string, yamlFile io.Reader) error {
	decodedYAML := yaml.NewYAMLOrJSONDecoder(yamlFile, 4096)
	discoveryClient := clientset.Discovery()

	apiGroupResources, err := restmapper.GetAPIGroupResources(discoveryClient)
	if err != nil {
		return err
	}
	rm := restmapper.NewDiscoveryRESTMapper(apiGroupResources)

	for {
		ext := runtime.RawExtension{}
		if err := decodedYAML.Decode(&ext); err != nil {
			if err == io.EOF {
				break
			}
			return err
		}

		_, gvk, err := unstructured.UnstructuredJSONScheme.Decode(ext.Raw, nil, nil)
		if err != nil {
			logrus.WithError(err).Fatalf(err.Error())
			return err
		}

		mapping, err := rm.RESTMapping(gvk.GroupKind(), gvk.Version)
		if err != nil {
			return err
		}
		k8sRes := mapping.Resource

		restconfig := config
		restconfig.GroupVersion = &schema.GroupVersion{
			Group:   mapping.GroupVersionKind.Group,
			Version: mapping.GroupVersionKind.Version,
		}
		dynamicClient, err := dynamic.NewForConfig(restconfig)
		if err != nil {
			return err
		}

		var unstruct unstructured.Unstructured
		unstruct.Object = make(map[string]interface{})
		var blob interface{}
		if err := json.Unmarshal(ext.Raw, &blob); err != nil {
			return err
		}

		unstruct.Object = blob.(map[string]interface{})

		res := dynamicClient.Resource(k8sRes)
		nsRes := res.Namespace(namespace)

		createRes := nsRes
		if k8sRes.Resource == "namespaces" || k8sRes.Resource == "configmap" || k8sRes.Resource == "clusterrolebindings" || k8sRes.Resource == "clusterroles" {
			createRes = res
		}

		_, err = createRes.Create(context.Background(), &unstruct, metav1.CreateOptions{})
		if err != nil {
			if !errors.IsAlreadyExists(err) {
				return err
			}
		}
	}

	return nil
}
