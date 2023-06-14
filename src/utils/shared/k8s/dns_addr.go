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
	"fmt"
	"strings"

	v1 "k8s.io/api/core/v1"
)

func GetPodAddr(pod v1.Pod) string {
	// IPv4
	podIP := strings.ReplaceAll(pod.Status.PodIP, ".", "-")
	// IPv6
	podIP = strings.ReplaceAll(podIP, ":", "-")

	return fmt.Sprintf("%s.%s.pod.cluster.local", podIP, pod.Namespace)
}
