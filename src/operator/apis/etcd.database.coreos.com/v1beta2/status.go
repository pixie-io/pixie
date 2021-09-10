// Copyright 2017 The etcd-operator Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// SPDX-License-Identifier: Apache-2.0

package v1beta2

import (
	"fmt"
	"time"

	v1 "k8s.io/api/core/v1"
)

type ClusterPhase string
type ClusterConditionType string

const (
	ClusterPhaseNone     ClusterPhase = ""
	ClusterPhaseCreating              = "Creating"
	ClusterPhaseRunning               = "Running"
	ClusterPhaseFailed                = "Failed"

	// See ./doc/user/conditions_and_events.md
	ClusterConditionAvailable  ClusterConditionType = "Available"
	ClusterConditionRecovering                      = "Recovering"
	ClusterConditionScaling                         = "Scaling"
	ClusterConditionUpgrading                       = "Upgrading"
)

type ClusterStatus struct {
	// Phase is the cluster running phase
	Phase  ClusterPhase `json:"phase"`
	Reason string       `json:"reason,omitempty"`

	// ControlPuased indicates the operator pauses the control of the cluster.
	ControlPaused bool `json:"controlPaused,omitempty"`

	// Condition keeps track of all cluster conditions, if they exist.
	Conditions []ClusterCondition `json:"conditions,omitempty"`

	// Size is the current size of the cluster
	Size int `json:"size"`

	// ServiceName is the LB service for accessing etcd nodes.
	ServiceName string `json:"serviceName,omitempty"`

	// ClientPort is the port for etcd client to access.
	// It's the same on client LB service and etcd nodes.
	ClientPort int `json:"clientPort,omitempty"`

	// Members are the etcd members in the cluster
	Members MembersStatus `json:"members"`
	// CurrentVersion is the current cluster version
	CurrentVersion string `json:"currentVersion"`
	// TargetVersion is the version the cluster upgrading to.
	// If the cluster is not upgrading, TargetVersion is empty.
	TargetVersion string `json:"targetVersion"`
}

// ClusterCondition represents one current condition of an etcd cluster.
// A condition might not show up if it is not happening.
// For example, if a cluster is not upgrading, the Upgrading condition would not show up.
// If a cluster is upgrading and encountered a problem that prevents the upgrade,
// the Upgrading condition's status will would be False and communicate the problem back.
type ClusterCondition struct {
	// Type of cluster condition.
	Type ClusterConditionType `json:"type"`
	// Status of the condition, one of True, False, Unknown.
	Status v1.ConditionStatus `json:"status"`
	// The last time this condition was updated.
	LastUpdateTime string `json:"lastUpdateTime,omitempty"`
	// Last time the condition transitioned from one status to another.
	LastTransitionTime string `json:"lastTransitionTime,omitempty"`
	// The reason for the condition's last transition.
	Reason string `json:"reason,omitempty"`
	// A human readable message indicating details about the transition.
	Message string `json:"message,omitempty"`
}

type MembersStatus struct {
	// Ready are the etcd members that are ready to serve requests
	// The member names are the same as the etcd pod names
	Ready []string `json:"ready,omitempty"`
	// Unready are the etcd members not ready to serve requests
	Unready []string `json:"unready,omitempty"`
}

func (cs *ClusterStatus) IsFailed() bool {
	if cs == nil {
		return false
	}
	return cs.Phase == ClusterPhaseFailed
}

func (cs *ClusterStatus) SetPhase(p ClusterPhase) {
	cs.Phase = p
}

func (cs *ClusterStatus) PauseControl() {
	cs.ControlPaused = true
}

func (cs *ClusterStatus) Control() {
	cs.ControlPaused = false
}

func (cs *ClusterStatus) UpgradeVersionTo(v string) {
	cs.TargetVersion = v
}

func (cs *ClusterStatus) SetVersion(v string) {
	cs.TargetVersion = ""
	cs.CurrentVersion = v
}

func (cs *ClusterStatus) SetReason(r string) {
	cs.Reason = r
}

func (cs *ClusterStatus) SetScalingUpCondition(from, to int) {
	c := newClusterCondition(ClusterConditionScaling, v1.ConditionTrue, "Scaling up", scalingMsg(from, to))
	cs.setClusterCondition(*c)
}

func (cs *ClusterStatus) SetScalingDownCondition(from, to int) {
	c := newClusterCondition(ClusterConditionScaling, v1.ConditionTrue, "Scaling down", scalingMsg(from, to))
	cs.setClusterCondition(*c)
}

func (cs *ClusterStatus) SetRecoveringCondition() {
	c := newClusterCondition(ClusterConditionRecovering, v1.ConditionTrue,
		"Disaster recovery", "Majority is down. Recovering from backup")
	cs.setClusterCondition(*c)

	cs.ClearCondition(ClusterConditionAvailable)
}

func (cs *ClusterStatus) SetUpgradingCondition(to string) {
	// TODO: show x/y members has upgraded.
	c := newClusterCondition(ClusterConditionUpgrading, v1.ConditionTrue,
		"Cluster upgrading", "upgrading to "+to)
	cs.setClusterCondition(*c)
}

func (cs *ClusterStatus) SetReadyCondition() {
	c := newClusterCondition(ClusterConditionAvailable, v1.ConditionTrue, "Cluster available", "")
	cs.setClusterCondition(*c)
}

func (cs *ClusterStatus) ClearCondition(t ClusterConditionType) {
	pos, _ := getClusterCondition(cs, t)
	if pos == -1 {
		return
	}
	cs.Conditions = append(cs.Conditions[:pos], cs.Conditions[pos+1:]...)
}

func (cs *ClusterStatus) setClusterCondition(c ClusterCondition) {
	pos, cp := getClusterCondition(cs, c.Type)
	if cp != nil &&
		cp.Status == c.Status && cp.Reason == c.Reason && cp.Message == c.Message {
		return
	}

	if cp != nil {
		cs.Conditions[pos] = c
	} else {
		cs.Conditions = append(cs.Conditions, c)
	}
}

func getClusterCondition(status *ClusterStatus, t ClusterConditionType) (int, *ClusterCondition) {
	for i, c := range status.Conditions {
		if t == c.Type {
			return i, &c
		}
	}
	return -1, nil
}

func newClusterCondition(condType ClusterConditionType, status v1.ConditionStatus, reason, message string) *ClusterCondition {
	now := time.Now().Format(time.RFC3339)
	return &ClusterCondition{
		Type:               condType,
		Status:             status,
		LastUpdateTime:     now,
		LastTransitionTime: now,
		Reason:             reason,
		Message:            message,
	}
}

func scalingMsg(from, to int) string {
	return fmt.Sprintf("Current cluster size: %d, desired cluster size: %d", from, to)
}
