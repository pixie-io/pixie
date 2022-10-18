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
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
)

// VizierLabelSelector returns a K8s selector that matches labels of all Pixie managed resources.
func VizierLabelSelector() metav1.LabelSelector {
	return metav1.LabelSelector{
		MatchLabels: make(map[string]string),
		MatchExpressions: []metav1.LabelSelectorRequirement{
			{
				Key:      "app",
				Operator: metav1.LabelSelectorOpIn,
				Values:   []string{"pixie-operator", "pl-monitoring"},
			},
		},
	}
}

// OperatorLabelSelector returns a K8s selector that matches the label of the
// Pixie Operator.
func OperatorLabelSelector() metav1.LabelSelector {
	return metav1.LabelSelector{
		MatchLabels: make(map[string]string),
		MatchExpressions: []metav1.LabelSelectorRequirement{
			{
				Key:      "olm.catalogSource",
				Operator: metav1.LabelSelectorOpIn,
				Values:   []string{"pixie-operator-index"},
			},
		},
	}
}
