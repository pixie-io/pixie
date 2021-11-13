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

package authenv

import (
	"github.com/spf13/viper"

	"px.dev/pixie/src/cloud/profile/profilepb"
	"px.dev/pixie/src/shared/services/env"
)

// AuthEnv is the authenv use for the Authentication service.
type AuthEnv interface {
	env.Env
	ProfileClient() profilepb.ProfileServiceClient
	OrgClient() profilepb.OrgServiceClient
}

// Impl is an implementation of the AuthEnv interface
type Impl struct {
	*env.BaseEnv
	profileClient profilepb.ProfileServiceClient
	orgClient     profilepb.OrgServiceClient
}

// NewWithDefaults creates a new auth authenv with defaults.
func NewWithDefaults() (*Impl, error) {
	pc, err := newProfileServiceClient()
	if err != nil {
		return nil, err
	}
	oc, err := newOrgServiceClient()
	if err != nil {
		return nil, err
	}

	return New(pc, oc)
}

// New creates a new auth authenv.
func New(pc profilepb.ProfileServiceClient, oc profilepb.OrgServiceClient) (*Impl, error) {
	return &Impl{env.New(viper.GetString("domain_name")), pc, oc}, nil
}

// ProfileClient returns the authenv's profile client.
func (e *Impl) ProfileClient() profilepb.ProfileServiceClient {
	return e.profileClient
}

// OrgClient returns the authenv's org client.
func (e *Impl) OrgClient() profilepb.OrgServiceClient {
	return e.orgClient
}
