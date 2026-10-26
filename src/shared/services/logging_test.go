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

package services

import (
	"testing"

	log "github.com/sirupsen/logrus"
	"github.com/spf13/pflag"
	"github.com/spf13/viper"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"
)

func TestSetupServiceLogging(t *testing.T) {
	SetupCommonFlags()

	tests := []struct {
		name          string
		logLevel      string
		expectedLevel log.Level
	}{
		{
			name:          "default",
			logLevel:      "",
			expectedLevel: log.InfoLevel,
		},
		{
			name:          "debug",
			logLevel:      "debug",
			expectedLevel: log.DebugLevel,
		},
		{
			name:          "error",
			logLevel:      "error",
			expectedLevel: log.ErrorLevel,
		},
		{
			name:          "case insensitive",
			logLevel:      "WARN",
			expectedLevel: log.WarnLevel,
		},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			origLevel := log.GetLevel()
			t.Cleanup(func() {
				viper.Reset()
				log.SetLevel(origLevel)
			})

			require.NoError(t, viper.BindPFlag("log_level", pflag.Lookup("log_level")))
			if test.logLevel != "" {
				viper.Set("log_level", test.logLevel)
			}

			SetupServiceLogging()
			assert.Equal(t, test.expectedLevel, log.GetLevel())
		})
	}
}

func TestSetupServiceLogging_InvalidLevel(t *testing.T) {
	origLevel := log.GetLevel()
	origExitFunc := log.StandardLogger().ExitFunc
	t.Cleanup(func() {
		viper.Reset()
		log.SetLevel(origLevel)
		log.StandardLogger().ExitFunc = origExitFunc
	})

	exitCode := 0
	log.StandardLogger().ExitFunc = func(code int) {
		exitCode = code
	}

	viper.Set("log_level", "verbose")
	SetupServiceLogging()
	assert.Equal(t, 1, exitCode)
}
