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

	"github.com/gofrs/uuid"
	"github.com/stretchr/testify/assert"

	"px.dev/pixie/src/cloud/config_manager/controllers"
	vizieryamls "px.dev/pixie/src/utils/template_generator/vizier_yamls"
)

func TestAddFeatureFlagToTemplate_Basic(t *testing.T) {
	client := controllers.NewVizierFeatureFlagClient("")
	tmplValues := &vizieryamls.VizierTmplValues{
		CustomPEMFlags: map[string]string{
			"PL_AN_EXISTING_FLAG": "false",
		},
	}

	controllers.AddFeatureFlagToTemplate(client, uuid.Must(uuid.NewV4()), "flag-name", "PL_FLAG_NAME", 3, tmplValues)
	expected := map[string]string{
		"PL_AN_EXISTING_FLAG": "false",
		"PL_FLAG_NAME":        "3",
	}
	assert.Equal(t, expected, tmplValues.CustomPEMFlags)
}

func TestAddFeatureFlagToTemplate_FlagSetByUser(t *testing.T) {
	client := controllers.NewVizierFeatureFlagClient("")
	tmplValues := &vizieryamls.VizierTmplValues{
		CustomPEMFlags: map[string]string{
			"PL_AN_EXISTING_FLAG": "false",
		},
	}

	controllers.AddFeatureFlagToTemplate(client, uuid.Must(uuid.NewV4()), "flag-name", "PL_AN_EXISTING_FLAG", true, tmplValues)
	expected := map[string]string{
		"PL_AN_EXISTING_FLAG": "false",
	}
	assert.Equal(t, expected, tmplValues.CustomPEMFlags)
}
