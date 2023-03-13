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
	VizierVersionTooOld:            "Vizier version is older by more than one major version and may no longer be supported. Please update to the latest version by redeploying or running `px update vizier`.",
	KernelVersionsIncompatible:     "Majority of nodes on the cluster have an incompatible Kernel version. Instrumentation may be incomplete. See https://docs.px.dev/installing-pixie/requirements/ for list of supported Kernel versions.",
	CloudConnectorFailedToConnect:  "Cloud connector failed to connect to Pixie Cloud. Please check the cloud address in the Vizier object to ensure it is correct and accessible within your firewall and network configurations.",
	CloudConnectorRegistering:      "Cloud connector is registering with Pixie Cloud. This may take a few minutes.",
	CloudConnectorInvalidDeployKey: "Invalid deploy key specified. If deploying via Helm or Manifest, please check your deployment key. Otherwise, ensure you have deployed with an authorized account and retry.",
	CloudConnectorBasicQueryFailed: "Unable to run basic healthcheck query on cluster. Refer to https://docs.px.dev/troubleshooting/ for troubleshooting recommendations.",
	CloudConnectorPodPending:       "Cloud connector pod is pending. If this status persists, investigate failures on the vizier-cloud-connector pod using `kubectl describe` and verify your cluster is not resource constrained.",
	CloudConnectorPodFailed:        "Cloud connector pod failed to start. If this status persists, investigate failures on the vizier-cloud-connector pod using `kubectl describe` and `kubectl logs`.",
	CloudConnectorMissing: "Cloud connector pod not found. Something is preventing the vizier-operator from deploying Pixie. " +
		"If this status persists, clobber and re-deploy your Pixie instance.",
	MetadataPVCMissing: "The PVC requested by Pixie cannot be found. The vizier-metadata service is unable to start without the PVC. " +
		"If this status persists, ensure that PersistentVolumes may be mounted in your cluster and clobber and re-deploy your Pixie instance.",
	MetadataPVCStorageClassUnavailable: "The PVC requested by Pixie cannot be created successfully: cluster lacks PersistentVolumes or dynamic storage provisioning. " +
		"See https://kubernetes.io/docs/concepts/storage/persistent-volumes/#lifecycle-of-a-volume-and-claim for info on setting up this feature on the cluster.",
	MetadataPVCPendingBinding:     "The PVC requested by Pixie is still Pending. If stuck in this status, investigate the status of PVC in the Vizier namespace (default `pl`) using `kubectl describe`.",
	MetadataStatefulSetPodPending: "The vizier-metadata pod is still Pending. This likely means that the PVC is unable to be mounted. Try switching to useEtcdOperator to fix this problem.",
	ControlPlaneFailedToScheduleBecauseOfTaints: "The Vizier control plane could not be scheduled because taints exist on " +
		"every node. Consider removing taints from some nodes or manually adding tolerations to each deployment in Vizier using the `patches` or `nodeSelector` flags.",
	ControlPlaneFailedToSchedule: "Vizier control plane pods failed to schedule. Investigate the failures of non-Ready pods in the Vizier namespace (default `pl`) using `kubectl describe`. Refer to https://docs.px.dev/troubleshooting/ for troubleshooting recommendations.",
	ControlPlanePodsPending:      "Vizier control plane pods are still pending. If this status persists, investigate details of Pending pods in the Vizier namespace (default `pl`) using `kubectl describe`. Refer to https://docs.px.dev/troubleshooting/ for troubleshooting recommendations.",
	ControlPlanePodsFailed:       "Vizier control plane pods are failing. If this status persists, investigate details on the Failed pods in the Vizier namespace (default pl) using `kubectl describe`. Refer to https://docs.px.dev/troubleshooting/ for troubleshooting recommendations.",
	NATSPodPending:               "NATS message bus pods are still pending. If this status persists, investigate failures on the Pending NATS pods in the Vizier namespace (default `pl`).",
	NATSPodMissing:               "NATS message bus pods are missing. If this status persists, clobber and redeploy this Pixie instance.",
	NATSPodFailed:                "NATS message bus pods have failed. Investigate failures on the Pending NATS pods in the Vizier namespace (default `pl`).",
	EtcdPodsMissing:              "etcd pods are missing. If this status persists, clobber and redeploy this Pixie instance.",
	EtcdPodsCrashing:             "etcd pods are in CrashLoopBackOff, likely due to loss of quorum. If this status persists, clobber and redeploy this Pixie instance.",
	UnableToConnectToCloud:       "Failed to connect to Pixie Cloud. Please check the cloud address (and optional dev cloud namespace) in the Vizier object to ensure it is correct and accessible within your firewall and network configurations.",
	PEMsSomeInsufficientMemory: "Some PEMs are failing to schedule due to insufficient memory available on the nodes. You will not be able to receive data from those failing nodes. " +
		"Free up memory on those nodes to start scraping Pixie data from those nodes.",
	PEMsAllInsufficientMemory: "None of the PEMs can schedule due to insufficient memory available on the nodes. " +
		"Free up memory on all nodes to enable Pixie to schedule.",
	PEMsMissing: "Cannot find any PEMs in the Vizier namespace (default `pl`). You will not receive data until some PEMs show up. " +
		"If this problem persists, clobber and re-deploy your Pixie instance",
	PEMsHighFailureRate: "PEMs are experiencing a high crash rate. Your Pixie experience will be degraded while this occurs. If PEMs are getting OOMKilled, increase your PEM memory limits using the `pemMemoryLimit` flag.",
	PEMsAllFailing:      "PEMs are all crashing. If PEMs are getting OOMKilled, increase your PEM memory limits using the `pemMemoryLimit` flag. Otherwise, consider filing a bug so someone can address your problem: https://github.com/pixie-io/pixie",
	TLSCertsExpired:     "Service TLS certs are expired. If using the operator, the certs will be auto-regenerated. Otherwise, please redeploy Vizier.",
}

// VizierReason is the reason that Vizier is in its current state.
// All VizierReason values should be included in this file.
type VizierReason string

// GetMessage gets the human-readable message for a Vizier status reason.
func (reason VizierReason) GetMessage() string {
	msg := reasonToMessageMap[reason]
	if msg == "" {
		msg = string(reason)
	}
	return msg
}

const (
	// VizierVersionTooOld occurs when the running Vizier version is more than one major version too old.
	VizierVersionTooOld VizierReason = "VizierVersionOld"
	// KernelVersionsIncompatible occurs when the majority of nodes have an incompatible kernel version.
	KernelVersionsIncompatible VizierReason = "KernelVersionsIncompatible"

	// CloudConnectorFailedToConnect occurs when the cloud connector is unable to connect to the specified cloud addr.
	CloudConnectorFailedToConnect VizierReason = "CloudConnectFailed"
	// CloudConnectorInvalidDeployKey occurs when the cloud connector has an invalid deploy key. This will prevent
	// the Vizier from properly registering.
	CloudConnectorInvalidDeployKey VizierReason = "InvalidDeployKey"
	// CloudConnectorBasicQueryFailed occurs when the cloud connector is fully connected, but fails to run basic queries.
	CloudConnectorBasicQueryFailed VizierReason = "BasicQueryFailed"
	// CloudConnectorPodPending occurs when a cloud connector is in the Pending Kubeneretes PodPhase.
	CloudConnectorPodPending VizierReason = "CloudConnectorPodPending"
	// CloudConnectorPodFailed occurs when a cloud connector pod is in the Failed Kubernetes PodPhase.
	CloudConnectorPodFailed VizierReason = "CloudConnectorPodFailed"
	// CloudConnectorMissing occurs when a cloud connector pod doesn't exist for a cluster.
	CloudConnectorMissing VizierReason = "CloudConnectorMissing"
	// CloudConnectorRegistering occurs when the cloud connector is still registering with Pixie Cloud.
	CloudConnectorRegistering VizierReason = "CloudConnectorRegistering"

	// MetadataPVCMissing occurs when the operator cannot find the metadata PVC.
	MetadataPVCMissing VizierReason = "MetadataPVCMissing"
	// MetadataPVCStorageClassUnavailable occurs when the Metadata PVC is stuck pending because the storage class does not exist.
	MetadataPVCStorageClassUnavailable VizierReason = "MetadataPVCStorageClassUnavailable"
	// MetadataPVCPendingBinding occurs when the Metadata PVC is still pending, but the spec is requesting a valid Storage class.
	MetadataPVCPendingBinding VizierReason = "MetadataPVCPendingBinding"
	// MetadataStatefulSetPodPending occurs when the stateful metadata pod is stuck pending.
	MetadataStatefulSetPodPending VizierReason = "MetadataStatefulSetPodPending"

	// ControlPlanePodsPending occurs when one or more control plane pods are pending.
	ControlPlanePodsPending VizierReason = "ControlPlanePodsPending"
	// ControlPlanePodsFailed occurs when one or more control plane pods are failing, but none are pending.
	ControlPlanePodsFailed VizierReason = "ControlPlanePodsFailed"
	// ControlPlaneFailedToSchedule occurs when a pod in the control plane cannot be scheduled. More specific states are enumerated in separate reasons below.
	ControlPlaneFailedToSchedule VizierReason = "ControlPlaneFailedToSchedule"
	// ControlPlaneFailedToScheduleBecauseOfTaints occurs when a pod in the control plane could not be scheduled because of restrictive taints.
	ControlPlaneFailedToScheduleBecauseOfTaints VizierReason = "ControlPlaneFailedToScheduleBecauseOfTaints"

	// NATSPodPending occurs when the nats pod is pending.
	NATSPodPending VizierReason = "NATSPodPending"
	// NATSPodMissing occurs when the nats pod is missing.
	NATSPodMissing VizierReason = "NATSPodMissing"
	// NATSPodFailed occurs when the nats pod failed to start up.
	NATSPodFailed VizierReason = "NATSPodFailed"

	// EtcdPodsMissing when the etcd pods are missing.
	EtcdPodsMissing VizierReason = "EtcdPodsMissing"
	// EtcdPodsCrashing when the etcd pods are crashing.
	EtcdPodsCrashing VizierReason = "EtcdPodsCrashing"

	// UnableToConnectToCloud occurs when the Operator cannot make requests to the Pixie Cloud instance.
	UnableToConnectToCloud VizierReason = "UnableToConnectToCloud"

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

	// TLSCertsExpired occurs when the service TLS certs are expired or almost expired.
	TLSCertsExpired VizierReason = "TLSCertsExpired"
)
