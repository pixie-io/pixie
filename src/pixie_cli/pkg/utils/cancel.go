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

package utils

import (
	"context"
	"os"
	"os/signal"
)

// WithSignalCancellable returns a context that will automatically be cancelled
// when Ctrl+C is pressed.
func WithSignalCancellable(ctx context.Context) (context.Context, func()) {
	// trap Ctrl+C and call cancel on the context
	newCtx, cancel := context.WithCancel(ctx)
	c := make(chan os.Signal, 1)
	signal.Notify(c, os.Interrupt)
	cleanup := func() {
		signal.Stop(c)
		cancel()
	}

	go func() {
		select {
		case <-c:
			cancel()
		case <-newCtx.Done():
		}
	}()

	return newCtx, cleanup
}
