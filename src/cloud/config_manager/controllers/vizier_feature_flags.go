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
	"strconv"
	"time"

	"github.com/gofrs/uuid"
	log "github.com/sirupsen/logrus"
	"gopkg.in/launchdarkly/go-sdk-common.v2/lduser"
	ld "gopkg.in/launchdarkly/go-server-sdk.v5"

	vizieryamls "px.dev/pixie/src/utils/template_generator/vizier_yamls"
)

type featureFlag struct {
	FeatureFlagName string
	VizierFlagName  string
	DefaultValue    interface{}
}

var availableFeatureFlags = []*featureFlag{
	{
		FeatureFlagName: "java-profiler",
		VizierFlagName:  "PL_PROFILER_JAVA_SYMBOLS",
		DefaultValue:    true,
	},
	{
		FeatureFlagName: "probe-static-tls-binaries",
		VizierFlagName:  "PX_TRACE_STATIC_TLS_BINARIES",
		DefaultValue:    true,
	},
	{
		FeatureFlagName: "debug-tls-sources",
		VizierFlagName:  "PX_DEBUG_TLS_SOURCES",
		DefaultValue:    false,
	},
}

// NewVizierFeatureFlagClient creates a LaunchDarkly feature flag client if the SDK key is provided,
// otherwise it creates a default feature flag client which always returns the default value.
// If LaunchDarkly times out, it will return the default feature flag client.
func NewVizierFeatureFlagClient(ldSDKKey string) VizierFeatureFlagClient {
	if ldSDKKey != "" {
		client, err := ld.MakeClient(ldSDKKey, time.Minute)
		if err == nil {
			log.Info("Successfully set up LaunchDarkly feature flag client")
			return &ldFeatureFlagClient{client}
		}
		log.WithError(err).Error("Could not create LaunchDarkly client, proceeding without LaunchDarkly")
	}
	log.Info("Setting up default feature flag client, skipping LaunchDarkly")
	return &defaultFeatureFlagClient{}
}

// VizierFeatureFlagClient is an interface for getting feature flags.
// It can be implemented with LaunchDarkly if LaunchDarkly is enabled.
type VizierFeatureFlagClient interface {
	BoolFlag(flagName string, orgID uuid.UUID, defaultVal bool) (bool, error)
	IntFlag(flagName string, orgID uuid.UUID, defaultVal int) (int, error)
}

// defaultFeatureFlagClient will always return the default value for a feature flag.
type defaultFeatureFlagClient struct{}

// BoolFlag returns the default value for the flag.
func (f *defaultFeatureFlagClient) BoolFlag(flagName string, orgID uuid.UUID, defaultVal bool) (bool, error) {
	return defaultVal, nil
}

// IntFlag returns the default value for the flag.
func (f *defaultFeatureFlagClient) IntFlag(flagName string, orgID uuid.UUID, defaultVal int) (int, error) {
	return defaultVal, nil
}

// ldFeatureFlagClient is the LaunchDarkly feature flag client, when launch darkly is enabled.
type ldFeatureFlagClient struct {
	client *ld.LDClient
}

// BoolFlag returns the value for the flag based on LaunchDarkly's value for the cluster/org.
func (f *ldFeatureFlagClient) BoolFlag(flagName string, orgID uuid.UUID, defaultVal bool) (bool, error) {
	// Using NewUserBuilder so we can easily add custom attributes here in the future.
	user := lduser.NewUserBuilder(orgID.String()).Build()
	res, err := f.client.BoolVariation(flagName, user, defaultVal)
	return res, err
}

// IntFlag returns the value for the flag based on LaunchDarkly's value for the cluster/org.
func (f *ldFeatureFlagClient) IntFlag(flagName string, orgID uuid.UUID, defaultVal int) (int, error) {
	// Using NewUserBuilder so we can easily add custom attributes here in the future.
	user := lduser.NewUserBuilder(orgID.String()).Build()
	res, err := f.client.IntVariation(flagName, user, defaultVal)
	return res, err
}

// AddFeatureFlagsToTemplate adds any feature flags to the specified Vizier template.
// If the flag has already been specified by the user, we should use the value they provided.
func AddFeatureFlagsToTemplate(client VizierFeatureFlagClient, orgID uuid.UUID, tmplValues *vizieryamls.VizierTmplValues) {
	if tmplValues.CustomPEMFlags == nil {
		tmplValues.CustomPEMFlags = make(map[string]string)
	}
	for _, f := range availableFeatureFlags {
		AddFeatureFlagToTemplate(client, orgID, f.FeatureFlagName, f.VizierFlagName, f.DefaultValue, tmplValues)
	}
}

// AddFeatureFlagToTemplate adds an individual feature flag to the Vizier template.
func AddFeatureFlagToTemplate(client VizierFeatureFlagClient, orgID uuid.UUID, featureFlag string, pemFlag string, defaultVal interface{},
	tmplValues *vizieryamls.VizierTmplValues) {
	if _, hasValue := tmplValues.CustomPEMFlags[pemFlag]; !hasValue {
		switch dv := defaultVal.(type) {
		case bool:
			val, err := client.BoolFlag(featureFlag, orgID, dv)
			if err != nil {
				log.WithError(err).Errorf("Error getting feature flag value for %s", featureFlag)
				return
			}
			tmplValues.CustomPEMFlags[pemFlag] = strconv.FormatBool(val)
		case int:
			val, err := client.IntFlag(featureFlag, orgID, dv)
			if err != nil {
				log.WithError(err).Errorf("Error getting feature flag value for %s", featureFlag)
				return
			}
			tmplValues.CustomPEMFlags[pemFlag] = strconv.Itoa(val)
		default:
			log.Errorf("Unknown default value type %T for feature flag %s, skipping", dv, featureFlag)
		}
	} else {
		log.Infof("Skipping feature flag %s (%s), already specified in Vizier template", featureFlag, pemFlag)
	}
}
