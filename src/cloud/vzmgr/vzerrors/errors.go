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

package vzerrors

import (
	"errors"

	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"
)

var (
	// ErrDeploymentKeyNotFound is used when specified key cannot be located.
	ErrDeploymentKeyNotFound = errors.New("invalid deployment key")
	// ErrProvisionFailedVizierIsActive errors when the specified vizier is active and not disconnected.
	ErrProvisionFailedVizierIsActive = errors.New("provisioning failed because vizier with specified UID is already active")
	// ErrInternalDB is used for internal errors related to DB.
	ErrInternalDB = errors.New("internal database error")
)

// ToGRPCError converts vzmgr errors to grpc errors if possible.
func ToGRPCError(err error) error {
	switch err {
	case ErrProvisionFailedVizierIsActive:
		return status.Error(codes.ResourceExhausted, err.Error())
	case ErrDeploymentKeyNotFound:
		return status.Error(codes.NotFound, err.Error())
	case ErrInternalDB:
		return status.Error(codes.Internal, err.Error())
	}
	return status.Error(codes.Internal, err.Error())
}
