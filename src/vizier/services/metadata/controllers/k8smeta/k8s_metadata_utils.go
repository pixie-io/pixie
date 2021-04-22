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

package k8smeta

// Blank import necessary for kubeConfig to work.
// Genny drops this import so include it here instead.
import _ "k8s.io/client-go/plugin/pkg/client/auth/gcp"

//go:generate genny -in=k8s_metadata_utils.tmpl -out k8s_metadata_utils.gen.go gen "ReplacedResource=Pod,Service,Namespace,Endpoints,Node"
