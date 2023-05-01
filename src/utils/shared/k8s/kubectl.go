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
	"os/exec"
)

func KubectlCmd(args ...string) *exec.Cmd {
	cmd := exec.Command("kubectl", args...)
	if *kubeconfig != "" {
		cmd.Env = append(cmd.Environ(), fmt.Sprintf("KUBECONFIG=%s", *kubeconfig))
	}
	return cmd
}
