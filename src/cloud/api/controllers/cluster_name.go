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

package controllers

import (
	"fmt"
	"strings"
)

// PrettifyClusterName uses heuristics to try to generate a better looking cluster name.
func PrettifyClusterName(name string, expanded bool) string {
	name = strings.ToLower(name)
	switch {
	case strings.HasPrefix(name, "gke_"):
		splits := strings.Split(name, "_")
		// GKE names are <gke>_<project>_<region>_<cluster_name>_<our suffix>
		if len(splits) > 3 && len(splits[3]) > 0 {
			project := splits[1]
			name := fmt.Sprintf("gke:%s", strings.Join(splits[3:], "_"))
			if expanded {
				return fmt.Sprintf("%s (%s)", name, project)
			}
			return name
		}
	case strings.HasPrefix(name, "arn:"):
		// EKS names are "ARN::::CLUSTER/NAME"
		splits := strings.Split(name, ":")
		if len(splits) > 0 && len(splits[len(splits)-1]) > 0 {
			eksName := splits[len(splits)-1]
			sp := strings.Split(eksName, "/")
			if len(sp) > 1 && len(sp[1]) > 0 {
				eksName = sp[1]
			}
			return fmt.Sprintf("eks:%s", eksName)
		}
	case strings.HasPrefix(name, "aks-"):
		return fmt.Sprintf("aks:%s", strings.TrimPrefix(name, "aks-"))
	}
	return name
}
