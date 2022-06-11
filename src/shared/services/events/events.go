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

package events

// This file tracks all the events the backend produces to
// reduce the chance of a typo messing up the analytics.

const (
	// UserLoggedIn is the login event.
	UserLoggedIn = "Logged In"
	// UserSignedUp is the signup event.
	UserSignedUp = "Signed Up"
	// OrgCreated is the event for a new Org.
	OrgCreated = "Org Created"
	// SiteCreated is the event for a new site.
	SiteCreated = "Site Created"
	// VizierCreated is an event for a new Vizier instance.
	VizierCreated = "Vizier Created"
	// VizierStatusChange is an event for when a Vizier cluster's status changes.
	VizierStatusChange = "Vizier Status Change"
	// APIRequest is an event for when a request is made to the Pixie API using an API token.
	APIRequest = "API Request"
	// PluginEnabled is an event for when a user enables a plugin.
	PluginEnabled = "Plugin Enabled"
	// PluginDisabled is an event for when a user disables a plugin.
	PluginDisabled = "Plugin Disabled"
	// PluginRetentionScriptCreated is an event for when a retention script is created.
	PluginRetentionScriptCreated = "Plugin Retention Script Created"
	// PluginRetentionScriptUpdated is an event for when a retention script is updated.
	PluginRetentionScriptUpdated = "Plugin Retention Script Updated"
	// PluginRetentionScriptDeleted is an event for when a retention script is deleted.
	PluginRetentionScriptDeleted = "Plugin Retention Script Deleted"
)
