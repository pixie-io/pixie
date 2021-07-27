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

package utils_test

import (
	"fmt"
	"testing"

	"github.com/stretchr/testify/assert"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"

	"px.dev/pixie/src/shared/services/utils"
)

func TestAtomicError(t *testing.T) {
	tests := []struct {
		Name         string
		Err          error
		SecondErr    error
		UseSecondErr bool
	}{
		{
			Name:         "nil error",
			Err:          nil,
			UseSecondErr: false,
		},
		{
			Name:         "fmt.Error",
			Err:          fmt.Errorf("test error"),
			UseSecondErr: false,
		},
		{
			Name:         "grpc status",
			Err:          status.Error(codes.Unknown, "grpc test error"),
			UseSecondErr: false,
		},
		{
			Name:         "nil then fmt.Error",
			Err:          nil,
			SecondErr:    fmt.Errorf("test error"),
			UseSecondErr: true,
		},
		{
			Name:         "fmt.Error then grpc status",
			Err:          fmt.Errorf("test error"),
			SecondErr:    status.Error(codes.Unknown, "grpc test error"),
			UseSecondErr: true,
		},
	}

	for _, test := range tests {
		t.Run(test.Name, func(t *testing.T) {
			e := utils.AtomicError{}
			e.Store(test.Err)
			assert.Equal(t, test.Err, e.Load())

			if test.UseSecondErr {
				e.Store(test.SecondErr)
				assert.Equal(t, test.SecondErr, e.Load())
			}
		})
	}
}
