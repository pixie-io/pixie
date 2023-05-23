/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

package k8s

import (
	"bytes"
	"context"
	"encoding/json"
	"errors"
	"flag"
	"fmt"
	"io"
	"regexp"
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
	"k8s.io/klog/v2"
)

func init() {
	// Suppress k8s log output.
	klog.InitFlags(nil)
	err := flag.Set("logtostderr", "false")
	if err != nil {
		log.WithError(err).Error("Failed to set flag")
	}
	err = flag.Set("v", "9")
	if err != nil {
		log.WithError(err).Error("Failed to set flag")
	}
	err = flag.Set("alsologtostderr", "false")
	if err != nil {
		log.WithError(err).Error("Failed to set flag")
	}

	// Suppress k8s log output.
	klog.SetOutput(io.Discard)
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
func ApplyYAML(clientset kubernetes.Interface, config *rest.Config, namespace string, yamlFile io.Reader, allowUpdate bool) error {
	return ApplyYAMLForResourceTypes(clientset, config, namespace, yamlFile, []string{}, allowUpdate)
}

// KeyValueStringToMap converts a user-inputted label string (label1=value,label2=value2) into a string map.
// Supports values that contain commas, as long as they are surrounded by quotes.
// eg. (label1="valuea,valueb",label2=value2)
func KeyValueStringToMap(labels string) (map[string]string, error) {
	labelMap := make(map[string]string)
	if labels == "" {
		return labelMap, nil
	}

	// Support values that contain commas by replacing all values that are in quotes with a placeholder, and then replacing the placeholder after splitting on commas.
	quotedReplMap := make(map[string]string)

	pattern, err := regexp.Compile(`"([^"]+)"`)
	if err != nil {
		return labelMap, err
	}
	quotedMatches := pattern.FindAllStringSubmatch(labels, -1)
	for i, quotedMatch := range quotedMatches {
		if len(quotedMatch) < 2 {
			continue
		}
		match := quotedMatch[1]
		repl := fmt.Sprintf("__val%d__", i)
		quotedReplMap[repl] = match
		labels = strings.Replace(labels, fmt.Sprintf(`"%s"`, match), repl, 1)
	}

	splitString := strings.Split(labels, ",")
	for _, labelPair := range splitString {
		splitLabel := strings.Split(labelPair, "=")
		if len(splitLabel) != 2 || splitLabel[0] == "" || splitLabel[1] == "" {
			return nil, errors.New("Label string is malformed")
		}
		if _, ok := quotedReplMap[splitLabel[0]]; ok {
			return nil, errors.New("Can not have quoted key, only quoted values")
		}
		val := splitLabel[1]
		if origVal, ok := quotedReplMap[splitLabel[1]]; ok {
			val = origVal
		}
		labelMap[splitLabel[0]] = val
	}
	return labelMap, nil
}

// ApplyYAMLForResourceTypes only applies the specified types in the given YAML file.
func ApplyYAMLForResourceTypes(clientset kubernetes.Interface, config *rest.Config, namespace string, yamlFile io.Reader, allowedResources []string, allowUpdate bool) error {
	resources, err := GetResourcesFromYAML(yamlFile)
	if err != nil {
		return err
	}

	return ApplyResources(clientset, config, resources, namespace, allowedResources, allowUpdate)
}

// Resource is an unstructured resource object with a group kind mapping.
type Resource struct {
	Object *unstructured.Unstructured
	GVK    *schema.GroupVersionKind
}

// GetResourcesFromYAML parses the YAMLs into K8s resource objects that can be passed to the API.
func GetResourcesFromYAML(yamlFile io.Reader) ([]*Resource, error) {
	resources := make([]*Resource, 0)

	decodedYAML := yaml.NewYAMLOrJSONDecoder(yamlFile, 4096)

	for {
		ext := runtime.RawExtension{}
		err := decodedYAML.Decode(&ext)

		if err != nil && err == io.EOF {
			break
		} else if err != nil {
			return nil, err
		}
		if ext.Raw == nil {
			continue
		}

		_, gvk, err := unstructured.UnstructuredJSONScheme.Decode(ext.Raw, nil, nil)
		if err != nil {
			log.WithError(err).Fatalf(err.Error())
			return nil, err
		}

		var unstructRes unstructured.Unstructured
		unstructRes.Object = make(map[string]interface{})
		var unstructBlob interface{}

		err = json.Unmarshal(ext.Raw, &unstructBlob)
		if err != nil {
			return nil, err
		}

		unstructRes.Object = unstructBlob.(map[string]interface{})

		resources = append(resources, &Resource{
			Object: &unstructRes,
			GVK:    gvk,
		})
	}

	return resources, nil
}

// ApplyResources applies the following resources to the give namespace/cluster.
func ApplyResources(clientset kubernetes.Interface, config *rest.Config, resources []*Resource, namespace string, allowedResources []string, allowUpdate bool) error {
	discoveryClient := clientset.Discovery()

	apiGroupResources, err := restmapper.GetAPIGroupResources(discoveryClient)
	if err != nil {
		return err
	}
	rm := restmapper.NewDiscoveryRESTMapper(apiGroupResources)

	for _, resource := range resources {
		mapping, err := rm.RESTMapping(resource.GVK.GroupKind(), resource.GVK.Version)
		if err != nil {
			return err
		}

		k8sRes := mapping.Resource.Resource
		if len(allowedResources) != 0 {
			validResource := false
			for _, res := range allowedResources {
				if res == k8sRes {
					validResource = true
				}
			}
			if !validResource {
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

		res := dynamicClient.Resource(mapping.Resource)
		objNS := namespace
		if objNS == "" { // If no namespace specified, use the namespace from the resource.
			if nestedNS, ok, _ := unstructured.NestedString(resource.Object.Object, "metadata", "namespace"); ok {
				objNS = nestedNS
			}
		}
		nsRes := res.Namespace(objNS)

		createRes := nsRes
		if k8sRes == "validatingwebhookconfigurations" || k8sRes == "mutatingwebhookconfigurations" || k8sRes == "namespaces" || k8sRes == "configmap" || k8sRes == "clusterrolebindings" || k8sRes == "clusterroles" || k8sRes == "customresourcedefinitions" {
			createRes = res
		}

		_, err = createRes.Create(context.Background(), resource.Object, metav1.CreateOptions{})
		if err != nil {
			if !k8serrors.IsAlreadyExists(err) {
				return err
			} else if (k8sRes == "clusterroles" || k8sRes == "cronjobs") || allowUpdate {
				// TODO(michelle,vihang,philkuz) Update() fails on services and PVCs that are already running on the
				// cluster. We will need to fix this before we can successfully update those resources. K8s is unhappy
				// that we don't specify resourceVersion and clusterIP for services.
				_, err = createRes.Update(context.Background(), resource.Object, metav1.UpdateOptions{})
				if err != nil {
					log.WithError(err).Info("Could not update K8s resource")
				}
			}
		}
	}

	return nil
}
