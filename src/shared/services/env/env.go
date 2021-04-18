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

package env

import (
	"github.com/spf13/viper"
)

// Env is the interface that all sub-environments should implement.
type Env interface {
	JWTSigningKey() string
	Audience() string
}

// BaseEnv is the struct containing server state that is valid across multiple sessions
// for example, database connections and config information.
type BaseEnv struct {
	jwtSigningKey string
	audience      string
}

// New creates a new base environment use by all our services.
func New(audience string) *BaseEnv {
	return &BaseEnv{
		jwtSigningKey: viper.GetString("jwt_signing_key"),
		audience:      audience,
	}
}

// JWTSigningKey returns the JWT key.
func (e *BaseEnv) JWTSigningKey() string {
	return e.jwtSigningKey
}

// Audience returns the audience that should be associated with any JWT keys.
func (e *BaseEnv) Audience() string {
	return e.audience
}
