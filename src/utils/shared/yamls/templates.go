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

package yamls

import (
	"bytes"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"strings"
	"text/template"

	"github.com/Masterminds/sprig/v3"
	jsonpatch "github.com/evanphx/json-patch/v5"
	goyaml "gopkg.in/yaml.v2"
	"k8s.io/apimachinery/pkg/apis/meta/v1/unstructured"
	"k8s.io/apimachinery/pkg/runtime"
	"k8s.io/apimachinery/pkg/runtime/schema"
	"k8s.io/apimachinery/pkg/util/strategicpatch"
	"k8s.io/apimachinery/pkg/util/yaml"
	"k8s.io/kubectl/pkg/scheme"
	k8syaml "sigs.k8s.io/yaml"
)

var nonNamespacedKinds = []string{"Namespace", "ClusterRoleBinding", "ClusterRole"}
var templateKinds = []string{"DaemonSet", "Deployment", "StatefulSet"}

// YAMLTmplArguments is a wrapper around YAMLTmplValues.
type YAMLTmplArguments struct {
	Values *map[string]interface{}
	// Release values represent special fields that are filled out by Helm.
	Release *map[string]interface{}
}

// ExecuteTemplatedYAMLs takes a template YAML and applies the given template values to it.
func ExecuteTemplatedYAMLs(yamls []*YAMLFile, tmplValues *YAMLTmplArguments) ([]*YAMLFile, error) {
	// Execute the template on each of the YAMLs.
	executedYAMLs := make([]*YAMLFile, len(yamls))
	for i, y := range yamls {
		yamlFile := &YAMLFile{
			Name: y.Name,
		}

		if tmplValues == nil {
			yamlFile.YAML = y.YAML
		} else {
			executedYAML, err := executeTemplate(tmplValues, y.YAML)
			if err != nil {
				return nil, err
			}
			yamlFile.YAML = executedYAML
		}
		executedYAMLs[i] = yamlFile
	}

	return executedYAMLs, nil
}

// AddPatchesToYAML takes a K8s YAML and adds the given patches using a strategic merge.
func AddPatchesToYAML(inputYAML string, patches map[string]string) (string, error) {
	// Create ResourceNameMatcher functions for each patch.
	matchFns := make(map[string]TemplateMatchFn)
	for k := range patches {
		matchFns[k] = GenerateResourceNameMatcherFn(k)
	}

	combinedYAML, err := processYAML(inputYAML, func(gvk schema.GroupVersionKind, resourceKind string, unstructuredObj unstructured.Unstructured, currJSON []byte) ([]byte, error) {
		for k, v := range patches {
			if matchFns[k](unstructuredObj.Object, resourceKind) {
				creatorObj, err := scheme.Scheme.New(gvk)
				if err != nil {
					// Strategic merge patches are not supported for non-native K8s resources (custom CRDs).
					// We will need to perform a regular JSON patch instead.
					b, err := jsonpatch.MergePatch(currJSON, []byte(v))
					if err != nil {
						return currJSON, nil
					}
					return b, nil
				}

				b, err := strategicpatch.StrategicMergePatch(currJSON, []byte(v), creatorObj)
				if err != nil {
					return currJSON, nil
				}
				return b, nil
			}
		}

		return currJSON, nil
	})
	if err != nil {
		return "", err
	}

	return combinedYAML, nil
}

// toYAML taken from Helm:
// https://github.com/helm/helm/blob/4ee8db2208923ea1ca1e4cc3792b2a3e088b6e0d/pkg/engine/funcs.go#L72-L98
func toYAML(v interface{}) string {
	data, err := goyaml.Marshal(v)
	if err != nil {
		// Swallow errors inside of a template.
		return ""
	}
	return strings.TrimSuffix(string(data), "\n")
}

func required(str string, value string) (string, error) {
	if value != "" {
		return value, nil
	}
	return "", errors.New("Value is required")
}

func executeTemplate(tmplValues *YAMLTmplArguments, tmplStr string) (string, error) {
	funcMap := sprig.TxtFuncMap()
	funcMap["required"] = required
	funcMap["toYaml"] = toYAML

	tmpl, err := template.New("yaml").Funcs(funcMap).Parse(tmplStr)
	if err != nil {
		return "", err
	}
	var buf bytes.Buffer
	err = tmpl.Execute(&buf, tmplValues)
	if err != nil {
		return "", err
	}

	return buf.String(), nil
}

// TemplateMatchFn is a function used to determine whether or not the given resource should have the template applied.
type TemplateMatchFn func(obj map[string]interface{}, resourceKind string) bool

// GenerateResourceNameMatcherFn creates a matcher function for matching the resource if the resource's name matches matchValue.
func GenerateResourceNameMatcherFn(expectedName string) TemplateMatchFn {
	fn := func(obj map[string]interface{}, resourceKind string) bool {
		if md, ok := obj["metadata"]; ok {
			if name, nameOk := md.(map[string]interface{})["name"]; nameOk {
				if name == expectedName {
					return true
				}
			}
		}
		return false
	}
	return fn
}

// NamespaceScopeMatcher matches the resource if the resource is contained within a namespace.
func NamespaceScopeMatcher(obj map[string]interface{}, resourceKind string) bool {
	for _, r := range nonNamespacedKinds {
		if resourceKind == r {
			return false
		}
	}
	return true
}

// TemplateScopeMatcher matches the resource definition contains a template for deploying other resources.
func TemplateScopeMatcher(obj map[string]interface{}, resourceKind string) bool {
	for _, r := range templateKinds {
		if resourceKind == r {
			return true
		}
	}
	return false
}

// GenerateServiceAccountSubjectMatcher matches the resource definition containing a service account subject with the given name.
func GenerateServiceAccountSubjectMatcher(subjectName string) TemplateMatchFn {
	fn := func(obj map[string]interface{}, resourceKind string) bool {
		if subjects, ok := obj["subjects"]; ok {
			subjList, ok := subjects.([]interface{})
			if !ok {
				return false
			}

			for _, s := range subjList {
				subject, ok := s.(map[string]interface{})
				if !ok {
					continue
				}
				if subject["name"] == subjectName {
					return true
				}
			}
		}
		return false
	}
	return fn
}

// GenerateContainerNameMatcherFn creates a matcher function for matching the resource if the resource has a pod
// template with a container of the given name.
func GenerateContainerNameMatcherFn(expectedName string) TemplateMatchFn {
	fn := func(obj map[string]interface{}, resourceKind string) bool {
		// Check that spec.template.spec.containers[].name in the YAML matches the given name.
		spec := obj["spec"]
		if spec == nil {
			return false
		}

		tmpl := spec.(map[string]interface{})["template"]
		if tmpl == nil {
			return false
		}

		tmplSpec := tmpl.(map[string]interface{})["spec"]
		if tmplSpec == nil {
			return false
		}
		containers := tmplSpec.(map[string]interface{})["containers"]
		if containers == nil {
			return false
		}
		containersList, ok := containers.([]interface{})
		if !ok {
			return false
		}

		for _, c := range containersList {
			container, ok := c.(map[string]interface{})
			if !ok {
				continue
			}
			if container["name"] == expectedName {
				return true
			}
		}

		return false
	}

	return fn
}

// K8sTemplateOptions specifies how the templated YAML should be constructed, by specifying selectors for which resources should
// contain the template, how the placeholder should be patched in, and what that placeholder should be replaced with.
type K8sTemplateOptions struct {
	// TemplateMatcher is a function that returns whether or not the template should be applied to the resource.
	TemplateMatcher TemplateMatchFn
	// The JSON that should be patched in, with the placeholder values.
	Patch string
	// Placeholder is the string in the YAML which should be replaced with the template value.
	Placeholder string
	// TemplateValue is the template string that should replace the placeholder.
	TemplateValue string
}

// Adds the placeholder to the object's JSON, by doing a strategic merge. The strategic merge will handle whether or not
// the patch should replace or add, depending on the K8s object schema.
func addPlaceholder(opt *K8sTemplateOptions, gvk schema.GroupVersionKind, originalJSON []byte) ([]byte, error) {
	creatorObj, err := scheme.Scheme.New(gvk)
	if err != nil {
		// Strategic merge patches are not supported for non-native K8s resources (custom CRDs).
		// We will need to perform a regular JSON patch instead.
		return jsonpatch.MergePatch(originalJSON, []byte(opt.Patch))
	}

	return strategicpatch.StrategicMergePatch(originalJSON, []byte(opt.Patch), creatorObj)
}

// TemplatizeK8sYAML takes a K8s YAML and templatizes the provided fields.
func TemplatizeK8sYAML(inputYAML string, tmplOpts []*K8sTemplateOptions) (string, error) {
	combinedYAML, err := processYAML(inputYAML, func(gvk schema.GroupVersionKind, resourceKind string, unstructuredObj unstructured.Unstructured, currJSON []byte) ([]byte, error) {
		var err error

		// Update image tags to account for custom registries.
		updateImageTags(unstructuredObj)

		json, err := unstructuredObj.MarshalJSON()
		if err != nil {
			return nil, err
		}

		for _, opt := range tmplOpts {
			if opt.TemplateMatcher != nil && !opt.TemplateMatcher(unstructuredObj.Object, resourceKind) {
				continue
			}

			json, err = addPlaceholder(opt, gvk, json)
			if err != nil {
				return nil, err
			}
		}

		return json, nil
	})
	if err != nil {
		return "", err
	}

	// Replace all placeholders with their template values.
	replacedStrings := make([]string, 0)
	for _, opt := range tmplOpts {
		replacedStrings = append(replacedStrings, []string{opt.Placeholder, opt.TemplateValue}...)
	}

	r := strings.NewReplacer(replacedStrings...)

	return r.Replace(combinedYAML), nil
}

func updateImageTags(unstructuredObj unstructured.Unstructured) {
	obj := unstructuredObj.Object
	spec := obj["spec"]
	if spec == nil {
		return
	}

	tmpl := spec.(map[string]interface{})["template"]
	if tmpl == nil {
		return
	}

	tmplSpec := tmpl.(map[string]interface{})["spec"]
	if tmplSpec == nil {
		return
	}
	containers := tmplSpec.(map[string]interface{})["containers"]
	if containers == nil {
		return
	}
	containersList, ok := containers.([]interface{})
	if !ok {
		return
	}

	for _, c := range containersList {
		container, ok := c.(map[string]interface{})
		if !ok {
			continue
		}
		container["image"] = templatizeImagePath(container["image"].(string))
	}

	initContainers := tmplSpec.(map[string]interface{})["initContainers"]
	if initContainers == nil {
		return
	}
	iContainersList, ok := initContainers.([]interface{})
	if !ok {
		return
	}

	for _, c := range iContainersList {
		container, ok := c.(map[string]interface{})
		if !ok {
			continue
		}

		container["image"] = templatizeImagePath(container["image"].(string))
	}
}

func templatizeImagePath(path string) string {
	newImage := strings.ReplaceAll(path, "/", "-")

	return fmt.Sprintf("{{ if .Values.registry }}{{ .Values.registry }}/%s{{else}}%s{{end}}", newImage, path)
}

type resourceProcessFn func(schema.GroupVersionKind, string, unstructured.Unstructured, []byte) ([]byte, error)

func processYAML(inputYAML string, processFn resourceProcessFn) (string, error) {
	// Read the YAML into K8s resources.
	yamlReader := strings.NewReader(inputYAML)

	decodedYAML := yaml.NewYAMLOrJSONDecoder(yamlReader, 4096)

	templatedYAMLs := make([]string, 0)

	for { // Loop through all objects in the YAML and process them.
		newYAML, err := processResourceInYAML(decodedYAML, processFn)
		if err != nil && err == io.EOF {
			break
		} else if err != nil {
			return "", err
		}

		templatedYAMLs = append(templatedYAMLs, newYAML)
	}
	// Concat the YAMLs.
	combinedYAML := ""
	for _, y := range templatedYAMLs {
		combinedYAML = ConcatYAMLs(combinedYAML, y)
	}

	return combinedYAML, nil
}

func processResourceInYAML(decodedYAML *yaml.YAMLOrJSONDecoder, processFn resourceProcessFn) (string, error) {
	// Read resource into JSON.
	ext := runtime.RawExtension{}
	err := decodedYAML.Decode(&ext)
	if err != nil {
		return "", err
	}

	_, gvk, err := unstructured.UnstructuredJSONScheme.Decode(ext.Raw, nil, nil)
	if err != nil {
		if len(ext.Raw) == 0 {
			return "", nil
		}
		return "", err
	}

	// Decode object into readable struct.
	var unstructuredOrig unstructured.Unstructured
	unstructuredOrig.Object = make(map[string]interface{})
	var unstructBlob interface{}

	err = json.Unmarshal(ext.Raw, &unstructBlob)
	if err != nil {
		return "", err
	}
	unstructuredOrig.Object = unstructBlob.(map[string]interface{})

	currJSON := ext.Raw
	currJSON, err = processFn(*gvk, gvk.Kind, unstructuredOrig, currJSON)
	if err != nil {
		return "", err
	}

	// Convert back to YAML.
	y, err := k8syaml.JSONToYAML(currJSON)
	if err != nil {
		return "", err
	}

	return string(y), nil
}
