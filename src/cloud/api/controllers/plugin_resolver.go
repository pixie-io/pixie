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
	"errors"
)

// PluginResolver is the resolver responsible for resolving plugins.
type PluginResolver struct {
	ID               string
	Name             string
	Description      string
	Logo             *string
	LatestVersion    string
	RetentionEnabled bool
	Enabled          bool
	EnabledVersion   *string
}

type pluginsArgs struct {
	Kind *string
}

type retentionPluginConfigArgs struct {
	ID string
}

// PluginConfigResolver is the resolver responsible for resolving plugin configs.
type PluginConfigResolver struct {
	Name        string
	Value       string
	Description string
}

// Plugins lists all of the plugins, filtered by kind if specified.
func (q *QueryResolver) Plugins(ctx context.Context, args *pluginsArgs) ([]*PluginResolver, error) {
	return nil, errors.New("Not yet implemented")
}

// RetentionPluginConfig lists the configured values for the given retention plugin.
func (q *QueryResolver) RetentionPluginConfig(ctx context.Context, args *retentionPluginConfigArgs) ([]*PluginConfigResolver, error) {
	return nil, errors.New("Not yet implemented")
}

type editablePluginConfig struct {
	Name  string
	Value string
}

type editablePluginConfigs struct {
	Configs []*editablePluginConfig
	Enabled *bool
}

type updateRetentionPluginConfigArgs struct {
	ID      string
	Enabled bool
	Configs editablePluginConfigs
}

// UpdateRetentionPluginConfig updates the configs for a retention plugin, including enabling/disabling the plugin.
func (q *QueryResolver) UpdateRetentionPluginConfig(ctx context.Context, args *updateRetentionPluginConfigArgs) (bool, error) {
	return false, errors.New("Not yet implemented")
}
