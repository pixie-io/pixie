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

package controllers_test

import (
	"testing"

	"github.com/stretchr/testify/assert"

	"px.dev/pixie/src/cloud/config_manager/controllers"
)

func TestAddDefaultTableStoreSizeMB_Basic(t *testing.T) {
	memory := "2Gi"
	pemFlags := make(map[string]string)

	controllers.AddDefaultTableStoreSize(memory, pemFlags)
	assert.Equal(t, 1, len(pemFlags))
	assert.Equal(t, "1228", pemFlags["PL_TABLE_STORE_DATA_LIMIT_MB"])
}

func TestAddDefaultTableStoreSizeMB_Empty(t *testing.T) {
	memory := ""
	pemFlags := make(map[string]string)

	controllers.AddDefaultTableStoreSize(memory, pemFlags)
	assert.Equal(t, 1, len(pemFlags))
	assert.Equal(t, "1228", pemFlags["PL_TABLE_STORE_DATA_LIMIT_MB"])
}

func TestAddDefaultTableStoreSize_AlreadyPresent(t *testing.T) {
	memory := "2Gi"
	pemFlags := map[string]string{
		"PL_TABLE_STORE_DATA_LIMIT_MB": "1024",
	}

	controllers.AddDefaultTableStoreSize(memory, pemFlags)
	assert.Equal(t, 1, len(pemFlags))
	assert.Equal(t, "1024", pemFlags["PL_TABLE_STORE_DATA_LIMIT_MB"])
}

func TestAddDefaultTableStoreSize_TooLowInput(t *testing.T) {
	memory := "1Mi"
	pemFlags := make(map[string]string)

	controllers.AddDefaultTableStoreSize(memory, pemFlags)
	assert.Equal(t, 0, len(pemFlags))
}
