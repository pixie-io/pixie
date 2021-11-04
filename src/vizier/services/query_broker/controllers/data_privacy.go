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

package controllers

import (
	"context"
	"fmt"

	"github.com/spf13/pflag"
	"github.com/spf13/viper"

	"px.dev/pixie/src/carnot/planner/distributedpb"

	pixie "px.dev/pixie/src/operator/apis/px.dev/v1alpha1"
)

func init() {
	pflag.String("data_access", "Full", "The data access level for queries. Options are 'Full' or 'Restricted' or 'PIIRestricted")
}

type vizierCachedDataPrivacy struct {
	dataAccess pixie.DataAccessLevel
}

// RedactionOptions returns the proto message containing options for redaction based on the cached data privacy level.
func (dp *vizierCachedDataPrivacy) RedactionOptions(ctx context.Context) (*distributedpb.RedactionOptions, error) {
	if dp.dataAccess == pixie.DataAccessFull {
		return nil, nil
	}
	return &distributedpb.RedactionOptions{
		UseFullRedaction:         dp.dataAccess == pixie.DataAccessRestricted,
		UsePxRedactPiiBestEffort: dp.dataAccess == pixie.DataAccessPIIRestricted,
	}, nil
}

// CreateDataPrivacyManager creates a privacy manager for the namespace.
func CreateDataPrivacyManager(ns string) (DataPrivacy, error) {
	dataAccessStr := viper.GetString("data_access")
	dataAccess := pixie.DataAccessLevel(dataAccessStr)
	switch dataAccess {
	case pixie.DataAccessFull, pixie.DataAccessRestricted, pixie.DataAccessPIIRestricted:
		return &vizierCachedDataPrivacy{dataAccess}, nil
	default:
		return nil, fmt.Errorf("Invalid DataAccess: '%s'", dataAccessStr)
	}
}
