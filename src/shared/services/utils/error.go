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
	"sync/atomic"
)

type errStruct struct {
	E error
}

// AtomicError allows atomic storing and loading of an error interface.
type AtomicError struct {
	v atomic.Value
}

// Load atomically loads the currently stored error. Note that if Load() is called before Store() it returns nil.
func (e *AtomicError) Load() error {
	err, ok := e.v.Load().(errStruct)
	if !ok {
		return nil
	}
	return err.E
}

// Store atomically stores a new error in the AtomicError.
func (e *AtomicError) Store(err error) {
	e.v.Store(errStruct{E: err})
}
