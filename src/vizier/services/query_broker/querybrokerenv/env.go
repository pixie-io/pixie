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

package querybrokerenv

import "px.dev/pixie/src/shared/services/env"

// QueryBrokerEnv is the interface for the Query Broker service environment.
type QueryBrokerEnv interface {
	// The address of the query broker
	Address() string
	// The SSL target hostname of the query broker
	SSLTargetName() string
	env.Env
}

// Impl is an implementation of the ApiEnv interface
type Impl struct {
	address       string
	sslTargetName string
	*env.BaseEnv
}

// New creates a new api env.
func New(qbAddress string, sslTargetName string, audience string) (*Impl, error) {
	return &Impl{
		qbAddress,
		sslTargetName,
		env.New(audience),
	}, nil
}

// Address returns the address of the query broker.
func (e *Impl) Address() string {
	return e.address
}

// SSLTargetName returns the SSL target hostname of the query broker.
func (e *Impl) SSLTargetName() string {
	return e.sslTargetName
}
