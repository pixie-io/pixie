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

package scriptrunner

import (
	"context"

	"px.dev/pixie/src/shared/cvmsgspb"
)

// A Source provides an initial set of cron scripts and sends incremental updates to that set.
type Source interface {
	// Start sends updates on updatesCh and returns the initial set of scripts which the updates are based on. It does not block.
	Start(baseCtx context.Context, updatesCh chan<- *cvmsgspb.CronScriptUpdate) (map[string]*cvmsgspb.CronScript, error)

	// Stop sending updates on the updatesCh provided in Start.
	// This method must not be called before Start.
	Stop()
}
