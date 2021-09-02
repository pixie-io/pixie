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

package status

// This file contains all possible reasons for the Vizier components.
// These are reported to the operator via statusz endpoints.

var reasonToMessageMap = map[VizierReason]string{
	"":                             "",
	CloudConnectorFailedToConnect:  "Cloud connector failed to connect to Pixie Cloud. Please check your cloud address.",
	CloudConnectorInvalidDeployKey: "Invalid deploy key specified. Please check that the deploy key is correct.",
	CloudConnectorBasicQueryFailed: "Unable to run basic healthcheck query on cluster.",

	VizierVersionTooOld: "Vizier version is older by more than one major version and may no longer be supported. Please update to the latest version.",
	ControlPlaneFailedToScheduleBecauseOfTaints: "The vizier control plane could not be scheduled because taints exist on " +
		"every node. Consider removing taints from some nodes or manually adding tolerations to each deployment in Vizier.",
	KernelVersionsIncompatible: "Majority of nodes on the cluster have an incompatible Kernel version. Instrumentation may be incomplete.",
}

// GetMessageFromReason gets the human-readable message for a vizier status reason.
func GetMessageFromReason(reason VizierReason) string {
	if msg, ok := reasonToMessageMap[reason]; ok {
		return msg
	}
	return ""
}

// VizierReason is the reason that Vizier is in its current state.
// All VizierReason values should be included in this file.
type VizierReason string

const (
	// VizierVersionTooOld is a status for when the running Vizier version is more than one major version too old.
	VizierVersionTooOld VizierReason = "VizierVersionOld"
	// KernelVersionsIncompatible is a status for when the majority of nodes have an incompatible kernel version.
	KernelVersionsIncompatible VizierReason = "KernelVersionsIncompatible"

	// CloudConnectorFailedToConnect is a status for when the cloud connector is unable to connect to the specified cloud addr.
	CloudConnectorFailedToConnect VizierReason = "CloudConnectFailed"
	// CloudConnectorInvalidDeployKey is a status for when the cloud connector has an invalid deploy key. This will prevent
	// the Vizier from properly registering.
	CloudConnectorInvalidDeployKey VizierReason = "InvalidDeployKey"
	// CloudConnectorBasicQueryFailed is a status for when the cloud connector is fully connected, but fails to run basic queries.
	CloudConnectorBasicQueryFailed VizierReason = "BasicQueryFailed"
	// CloudConnectorPodPending is a status when a cloud connector that is still in the Pending Kubernetes Phase.
	CloudConnectorPodPending VizierReason = "CloudConnectorPodPending"
	// CloudConnectorPodFailed is the status when a cloud connector pod is in the Failed Kubernetes Phase.
	CloudConnectorPodFailed VizierReason = "CloudConnectorPodFailed"
	// CloudConnectorMissing is the status when a cloud connector doesn't exist for a cluster.
	CloudConnectorMissing VizierReason = "CloudConnectorMissing"

	// MetadataPVCMissing occurs when the operator cannot find the metadata PVC.
	MetadataPVCMissing VizierReason = "MetadataPVCMissing"
	// MetadataPVCStorageClassUnavailable occurs when the Metadata PVC is stuck pending because the storage class does not exist.
	MetadataPVCStorageClassUnavailable VizierReason = "MetadataPVCStorageClassUnavailable"
	// MetadataPVCPendingBinding occurs when the Metadata PVC is still pending, but the spec is requesting a valid Storage class.
	MetadataPVCPendingBinding VizierReason = "MetadataPVCPendingBinding"

	// ControlPlanePodsPending occurs when one or more control plane pods are pending.
	ControlPlanePodsPending VizierReason = "ControlPlanePodsPending"
	// ControlPlanePodsFailed occurs when one or more control plane pods are failing, but none are pending.
	ControlPlanePodsFailed VizierReason = "ControlPlanePodsFailed"

	// NATSPodPending occurs when the nats pod is pending.
	NATSPodPending VizierReason = "NATSPodPending"
	// NATSPodMissing occurs when the nats pod is missing.
	NATSPodMissing VizierReason = "NATSPodMissing"
	// NATSPodFailed occurs when the nats pod failed to start up.
	NATSPodFailed VizierReason = "NATSPodFailed"

	// PEMsSomeInsufficientMemory occurs when some PEMs (strictly not all) fail to schedule due to insufficient memory. If all PEMs experience
	// insufficient memory, then the Reason should be PEMsAllInsufficientMemory.
	PEMsSomeInsufficientMemory VizierReason = "PEMsSomeInsufficientMemory"
	// PEMsAllInsufficientMemory occurs when all PEMs fail to schedule due to insufficient memory. If only some pods experience
	// insufficient memory, then the Reason should be PEMsSomeInsufficientMemory.
	PEMsAllInsufficientMemory VizierReason = "PEMsAllInsufficientMemory"
	// PEMsMissing occurs when the operator can't find PEMs in the monitoring namespace.
	PEMsMissing VizierReason = "PEMsMissing"

	// PEMsHighFailureRate occurs when a large portion of PEMs are hurting the stability of the cluster.
	PEMsHighFailureRate VizierReason = "PEMsHighFailureRate"
	// PEMsAllFailing occurs when a all PEMs are failing.
	PEMsAllFailing VizierReason = "PEMsAllFailing"

	// ControlPlaneFailedToSchedule occurs when a pod in the control plane cannot be scheduled. More specific states are enumerated in separate reasons below.
	ControlPlaneFailedToSchedule VizierReason = "ControlPlaneFailedToSchedule"
	// ControlPlaneFailedToScheduleBecauseOfTaints occurs when a pod in the control plane could not be scheduled because of restrictive taints.
	ControlPlaneFailedToScheduleBecauseOfTaints VizierReason = "ControlPlaneFailedToScheduleBecauseOfTaints"
)
