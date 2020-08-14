package k8s

import (
	"bytes"
	"context"
	"encoding/json"
	"flag"
	"io"
	"io/ioutil"

	log "github.com/sirupsen/logrus"
	"k8s.io/apimachinery/pkg/api/errors"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/apimachinery/pkg/apis/meta/v1/unstructured"
	"k8s.io/apimachinery/pkg/runtime"
	"k8s.io/apimachinery/pkg/runtime/schema"
	jsonserializer "k8s.io/apimachinery/pkg/runtime/serializer/json"
	"k8s.io/apimachinery/pkg/util/yaml"
	"k8s.io/client-go/dynamic"
	"k8s.io/client-go/kubernetes"
	"k8s.io/client-go/rest"
	"k8s.io/client-go/restmapper"
	"k8s.io/klog"
)

func init() {
	// Suppress k8s log output.
	klog.InitFlags(nil)
	flag.Set("logtostderr", "false")
	flag.Set("v", "9")
	flag.Set("alsologtostderr", "false")

	// Suppress k8s log output.
	klog.SetOutput(ioutil.Discard)
}

// ConvertResourceToYAML converts the given object to a YAML which can be applied.
func ConvertResourceToYAML(obj runtime.Object) (string, error) {
	buf := new(bytes.Buffer)
	e := jsonserializer.NewYAMLSerializer(jsonserializer.DefaultMetaFactory, nil, nil)
	err := e.Encode(obj, buf)
	if err != nil {
		return "", err
	}
	return buf.String(), nil
}

// ApplyYAML does the equivalent of a kubectl apply for the given yaml.
func ApplyYAML(clientset *kubernetes.Clientset, config *rest.Config, namespace string, yamlFile io.Reader) error {
	return ApplyYAMLForResourceTypes(clientset, config, namespace, yamlFile, []string{})
}

// ApplyYAMLForResourceTypes only applies the specified types in the given YAML file.
// This function is copied from https://stackoverflow.com/a/47139247 with major updates with
// respect to API changes and some clean up work.
func ApplyYAMLForResourceTypes(clientset *kubernetes.Clientset, config *rest.Config, namespace string, yamlFile io.Reader, allowedResources []string) error {
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
			log.WithError(err).Fatalf(err.Error())
			return err
		}

		mapping, err := rm.RESTMapping(gvk.GroupKind(), gvk.Version)
		if err != nil {
			return err
		}
		k8sRes := mapping.Resource

		if len(allowedResources) != 0 {
			validResource := false
			for _, res := range allowedResources {
				if res == k8sRes.Resource {
					validResource = true
				}
			}
			if validResource == false {
				continue // Don't apply this resource.
			}
		}

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
		if k8sRes.Resource == "podsecuritypolicies" || k8sRes.Resource == "namespaces" || k8sRes.Resource == "configmap" || k8sRes.Resource == "clusterrolebindings" || k8sRes.Resource == "clusterroles" {
			createRes = res
		}

		_, err = createRes.Create(context.Background(), &unstruct, metav1.CreateOptions{})
		if err != nil {
			if !errors.IsAlreadyExists(err) {
				return err
			} else if k8sRes.Resource == "clusterroles" || k8sRes.Resource == "cronjobs" { // Try to update the resource if it is a clusterrole or cronjob.
				_, err = createRes.Update(context.Background(), &unstruct, metav1.UpdateOptions{})
			}
		}
	}

	return nil
}
