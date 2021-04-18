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

package pg

import (
	"testing"

	"github.com/spf13/viper"
	"github.com/stretchr/testify/assert"
)

func TestDefaultDBURI(t *testing.T) {
	viper.Set("postgres_port", 5000)
	viper.Set("postgres_hostname", "postgres-host")
	viper.Set("postgres_db", "thedb")
	viper.Set("postgres_username", "user")
	viper.Set("postgres_password", "pass")

	t.Run("With SSL", func(t *testing.T) {
		viper.Set("postgres_ssl", true)
		assert.Equal(t, DefaultDBURI(), "postgres://user:pass@postgres-host:5000/thedb?sslmode=require")
	})

	t.Run("Without SSL", func(t *testing.T) {
		viper.Set("postgres_ssl", false)
		assert.Equal(t, DefaultDBURI(), "postgres://user:pass@postgres-host:5000/thedb?sslmode=disable")
	})
}
