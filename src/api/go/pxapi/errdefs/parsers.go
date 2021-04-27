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
	"fmt"

	"google.golang.org/grpc/codes"

	"px.dev/pixie/src/api/proto/vizierpb"
)

// ParseStatus parses the status field. Returns an error if exists.
func ParseStatus(s *vizierpb.Status) error {
	if s == nil || s.Code == int32(codes.OK) {
		return nil
	}
	if len(s.ErrorDetails) > 0 {
		details := s.ErrorDetails
		var errs []error
		hasCompilerErrors := false
		for _, d := range details {
			switch e := d.Error.(type) {
			case *vizierpb.ErrorDetails_CompilerError:
				errs = append(errs, newCompilerErrorWithDetails(e.CompilerError))
				hasCompilerErrors = true
			default:
				errs = append(errs, ErrInternal)
			}
		}
		if hasCompilerErrors {
			return newCompilerMultiError(errs...)
		}
		return newErrorGroup(errs...)
	}
	if s.Code == int32(codes.InvalidArgument) {
		return fmt.Errorf("%w: %s", ErrInvalidArgument, s.Message)
	}
	return ErrInternal
}
