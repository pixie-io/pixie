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

package bridge

import "errors"

var (
	// ErrMissingRegistrationMessage is produced if registration was not the first message.
	ErrMissingRegistrationMessage = errors.New("Expected registration message")
	// ErrBadRegistrationMessage ia produced if a malformed registration message is received.
	ErrBadRegistrationMessage = errors.New("Malformed registration message")
	// ErrRegistrationFailedUnknown is the error for vizier registration failure.
	ErrRegistrationFailedUnknown = errors.New("registration failed unknown")
	// ErrRegistrationFailedNotFound is the error for vizier registration failure when vizier is not found.
	ErrRegistrationFailedNotFound = errors.New("registration failed not found")
	// ErrRequestChannelClosed is an error returned when the streams have already been closed.
	ErrRequestChannelClosed = errors.New("request channel already closed")
)
