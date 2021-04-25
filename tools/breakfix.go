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

package breakfix

// This blank import exists solely so that we'd explicitly pull
// this in as a go_repository and prevent rules_docker from
// trying to import it.
// This is to work around https://github.com/bazelbuild/rules_docker/issues/1814
// For more context, also see https://github.com/bazelbuild/rules_docker/pull/1815
// and https://github.com/google/go-containerregistry/issues/997
import _ "github.com/google/go-containerregistry/pkg/v1" // bugfix
