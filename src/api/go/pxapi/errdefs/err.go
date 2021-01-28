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
 */
package errdefs

import "errors"

var (
	// ErrStreamAlreadyClosed is invoked when trying to read a stream that has been closed or cancelled.
	ErrStreamAlreadyClosed = errors.New("stream has already been closed")
	// ErrUnImplemented is used for unimplemented features.
	ErrUnImplemented = errors.New("unimplemented")
	// ErrInternalMissingTableMetadata specifies an internal error has occurred where the table metadata is missing.
	ErrInternalMissingTableMetadata = errors.New("internal error, missing table metadata")
	// ErrInternalDuplicateTableMetadata specifies an internal error has occurred where the table metadata has shown up multiple times.
	ErrInternalDuplicateTableMetadata = errors.New("internal error, duplicate table metadata")
	// ErrInternalMismatchedType specifies an internal error has occurred where the table types don't match up between metadata and the various batches.
	ErrInternalMismatchedType = errors.New("internal error, types don't match between metadata and row batch data")
	// ErrInternalUnImplementedType specifies an internal error has occurred where the types used by the Pixie API are not supported by this client version.
	// Most likely a client version update will fix the problem.
	ErrInternalUnImplementedType = errors.New("internal error, unimplemented type")
	// ErrInternalDataAfterEOS got data after EOS.
	ErrInternalDataAfterEOS = errors.New("internal error, got data after eos")
)
