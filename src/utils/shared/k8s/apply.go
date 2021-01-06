package k8s

import (
	"bytes"
	"context"
	"encoding/json"
	"errors"
	"flag"
	"io"
	"io/ioutil"
	"strings"

	log "github.com/sirupsen/logrus"
	k8serrors "k8s.io/apimachinery/pkg/api/errors"
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

// ApplyYAML does the equivalent of a kubectl apply for the given yaml. If allowUpdate is true, then we update the resource
// if it already exists.
func ApplyYAML(clientset *kubernetes.Clientset, config *rest.Config, namespace string, yamlFile io.Reader, allowUpdate bool, labels map[string]string) error {
	return ApplyYAMLForResourceTypes(clientset, config, namespace, yamlFile, []string{}, allowUpdate, labels)
}

// LabelStringToMap converts a user-inputted label string (label1=value,label2=value2) into a string map.
func LabelStringToMap(labels string) (map[string]string, error) {
	labelMap := make(map[string]string)
	splitString := strings.Split(labels, ",")
	for _, labelPair := range splitString {
		splitLabel := strings.Split(labelPair, "=")
		if len(splitLabel) != 2 || splitLabel[0] == "" || splitLabel[1] == "" {
			return nil, errors.New("Label string is malformed")
		}
		labelMap[splitLabel[0]] = splitLabel[1]
	}
	return labelMap, nil
}

// addLabelsToResource adds the given labels to the K8s resource.
func addLabelsToResource(labels map[string]string, res map[string]interface{}) map[string]interface{} {
	// Add the labels to the resource's labels.
	metadata := make(map[string]interface{})
	if md, ok := res["metadata"]; ok {
		castedMd, castOk := md.(map[string]interface{})
		if castOk {
			metadata = castedMd
		}
	}

	resLabels := make(map[string]interface{})
	if l, ok := metadata["labels"]; ok {
		castedLabel, castOk := l.(map[string]interface{})
		if castOk {
			resLabels = castedLabel
		}
	}

	for k, v := range labels {
		resLabels[k] = v
	}
	metadata["labels"] = resLabels

	// If it exists, recursively add the labels to the resource's template (for deployments/daemonsets).
	if spec, ok := res["spec"]; ok {
		castedSpec, castOk := spec.(map[string]interface{})
		if castOk {
			if tmpl, tmplOk := castedSpec["template"]; tmplOk {
				castedTmpl, tmplCastOk := tmpl.(map[string]interface{})
				if tmplCastOk {
					updatedTmpl := addLabelsToResource(labels, castedTmpl)
					castedSpec["template"] = updatedTmpl
					res["spec"] = castedSpec
				}
			}
		}
	}

	res["metadata"] = metadata
	return res
}

// ApplyYAMLForResourceTypes only applies the specified types in the given YAML file.
func ApplyYAMLForResourceTypes(clientset *kubernetes.Clientset, config *rest.Config, namespace string, yamlFile io.Reader, allowedResources []string, allowUpdate bool, labels map[string]string) error {
	if labels == nil {
		labels = make(map[string]string)
	}

	decodedYAML := yaml.NewYAMLOrJSONDecoder(yamlFile, 4096)
	discoveryClient := clientset.Discovery()

	apiGroupResources, err := restmapper.GetAPIGroupResources(discoveryClient)
	if err != nil {
		return err
	}
	rm := restmapper.NewDiscoveryRESTMapper(apiGroupResources)

	for {
		ext := runtime.RawExtension{}
		err := decodedYAML.Decode(&ext)

		if err != nil && err == io.EOF {
			break
		} else if err != nil {
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

		var unstructRes unstructured.Unstructured
		unstructRes.Object = make(map[string]interface{})
		var unstructBlob interface{}

		err = json.Unmarshal(ext.Raw, &unstructBlob)
		if err != nil {
			return err
		}

		unstructRes.Object = unstructBlob.(map[string]interface{})

		res := dynamicClient.Resource(k8sRes)
		nsRes := res.Namespace(namespace)

		// Apply any custom labels to the resource.
		unstructRes.Object = addLabelsToResource(labels, unstructRes.Object)

		createRes := nsRes
		if k8sRes.Resource == "podsecuritypolicies" || k8sRes.Resource == "namespaces" || k8sRes.Resource == "configmap" || k8sRes.Resource == "clusterrolebindings" || k8sRes.Resource == "clusterroles" {
			createRes = res
		}

		_, err = createRes.Create(context.Background(), &unstructRes, metav1.CreateOptions{})
		if err != nil {
			if !k8serrors.IsAlreadyExists(err) {
				return err
			} else if (k8sRes.Resource == "clusterroles" || k8sRes.Resource == "cronjobs") || allowUpdate {
				_, err = createRes.Update(context.Background(), &unstructRes, metav1.UpdateOptions{})
			}
		}
	}

	return nil
}
