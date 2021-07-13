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

package apienv_test

import (
	"testing"

	"github.com/spf13/viper"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"px.dev/pixie/src/cloud/api/apienv"
)

func TestNew(t *testing.T) {
	viper.Set("session_key", "a-key")
	env, err := apienv.New(nil, nil, nil, nil, nil, nil, nil, nil)
	require.NoError(t, err)
	assert.NotNil(t, env)
	assert.NotNil(t, env.CookieStore())
}

func TestNew_MissingSessionKey(t *testing.T) {
	viper.Set("session_key", "")
	env, err := apienv.New(nil, nil, nil, nil, nil, nil, nil, nil)
	assert.NotNil(t, err)
	assert.Nil(t, env)
}
