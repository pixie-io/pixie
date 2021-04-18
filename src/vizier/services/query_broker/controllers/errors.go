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

import "errors"

var (
	// ErrTracepointRegistrationFailed failed to register tracepoint.
	ErrTracepointRegistrationFailed = errors.New("failed to register tracepoints")
	// ErrTracepointDeletionFailed failed to delete tracepoint.
	ErrTracepointDeletionFailed = errors.New("failed to delete tracepoints")
	// ErrTracepointPending tracepoint is still pending.
	ErrTracepointPending = errors.New("tracepoints are still pending")
	// ErrConfigUpdateFailed failed to send the config update request to an agent.
	ErrConfigUpdateFailed = errors.New("failed to update config")
)
