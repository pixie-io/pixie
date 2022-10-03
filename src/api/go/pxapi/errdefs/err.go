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

package errdefs

import (
	"errors"
	"fmt"
)

var (
	// ErrStreamAlreadyClosed is invoked when trying to read a stream that has been closed or cancelled.
	ErrStreamAlreadyClosed = errors.New("stream has already been closed")
	// ErrClusterNotFound is invoked when trying to fetch information for a nonexistent cluster ID.
	ErrClusterNotFound = errors.New("cluster not found")
	// ErrUnImplemented is used for unimplemented features.
	ErrUnImplemented = errors.New("unimplemented")

	// ErrInvalidArgument specifies an unknown internal error has occurred.
	ErrInvalidArgument = errors.New("invalid/missing arguments")

	// ErrMissingDecryptionKey occurs if vizier sends encrypted table data without being asked to do so.
	ErrMissingDecryptionKey = errors.New("missing decryption key but got encrypted data")

	// ErrInternal specifies an unknown internal error has occurred.
	ErrInternal = errors.New("internal error")
	// ErrInternalMissingTableMetadata specifies an internal error has occurred where the table metadata is missing.
	ErrInternalMissingTableMetadata = createInternalError("missing table metadata")
	// ErrInternalDuplicateTableMetadata specifies an internal error has occurred where the table metadata has shown up multiple times.
	ErrInternalDuplicateTableMetadata = createInternalError("duplicate table metadata")
	// ErrInternalMismatchedType specifies an internal error has occurred where the table types don't match up between metadata and the various batches.
	ErrInternalMismatchedType = createInternalError("types don't match between metadata and row batch data")
	// ErrInternalUnImplementedType specifies an internal error has occurred where the types used by the Pixie API are not supported by this client version.
	// Most likely a client version update will fix the problem.
	ErrInternalUnImplementedType = createInternalError("unimplemented type")
	// ErrInternalDataAfterEOS got data after EOS.
	ErrInternalDataAfterEOS = createInternalError("got data after eos")

	// ErrCompilation is a generic PxL compilation error.
	ErrCompilation = errors.New("compilation error")

	// ErrMissingArtifact occurs when an artifact could not be found.
	ErrMissingArtifact = errors.New("missing artifact")
)

// MultiError is an interface to allow access to groups of errors.
type MultiError interface {
	error
	Errors() []error
}

type errorGroup struct {
	errs []error
}

func (e errorGroup) Error() string {
	s := "Multiple Errors: "
	for i, err := range e.errs {
		if i > 0 {
			s += ", "
		}
		s += err.Error()
	}
	return s
}

func (e errorGroup) Errors() []error {
	return e.errs
}

func newErrorGroup(errs ...error) error {
	e := errorGroup{
		errs: make([]error, len(errs)),
	}
	copy(e.errs, errs)
	return e
}

// IsInternalError checks to see if the error is an internal error.
func IsInternalError(e error) bool {
	return errors.Is(e, ErrInternal)
}

// IsCompilationError returns true if the error is a result of PxlCompilationError.
func IsCompilationError(e error) bool {
	return errors.Is(e, ErrCompilation)
}

func createInternalError(s string) error {
	return fmt.Errorf("%s : %w", s, ErrInternal)
}
