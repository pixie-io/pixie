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

// Generate the code for deep-copying the CRD in go.
//go:generate controller-gen object
// Generate the CRD YAMLs.
//go:generate controller-gen crd:trivialVersions=true rbac:roleName=operator-role webhook output:crd:artifacts:config=crd output:crd:dir:=../../../../../k8s/operator/crd/base
// Generate the clientset.
//go:generate client-gen --input=px.dev/v1alpha1 --clientset-name=versioned --go-header-file=/dev/null --input-base=px.dev/pixie/src/operator/apis --output-package=px.dev/pixie/src/operator/client

package v1alpha1

import (
	v1 "k8s.io/api/core/v1"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"

	"px.dev/pixie/src/shared/status"
)

// VizierSpec defines the desired state of Vizier
type VizierSpec struct {
	// Version is the desired version of the Vizier instance.
	Version string `json:"version,omitempty"`
	// DeployKey is the deploy key associated with the Vizier instance. This is used to link the Vizier to a
	// specific user/org. This is required unless specifying a CustomDeployKeySecret.
	DeployKey string `json:"deployKey,omitempty"`
	// CustomDeployKeySecret is the name of the secret where the deploy key is stored.
	CustomDeployKeySecret string `json:"customDeployKeySecret,omitempty"`
	// DisableAutoUpdate specifies whether auto update should be enabled for the Vizier instance.
	DisableAutoUpdate bool `json:"disableAutoUpdate,omitempty"`
	// UseEtcdOperator specifies whether the metadata service should use etcd for storage.
	UseEtcdOperator bool `json:"useEtcdOperator,omitempty"`
	// ClusterName is a name for the Vizier instance, usually specifying which cluster the Vizier is
	// deployed to. If not specified, a random name will be generated.
	ClusterName string `json:"clusterName,omitempty"`
	// CloudAddr is the address of the cloud instance that the Vizier should be pointing to.
	CloudAddr string `json:"cloudAddr,omitempty"`
	// DevCloudNamespace should be specified only for dev versions of Pixie cloud which have no ingress to help
	// redirect traffic to the correct service. The DevCloudNamespace is the namespace that the dev Pixie cloud is
	// running on, for example: "plc-dev".
	DevCloudNamespace string `json:"devCloudNamespace,omitempty"`
	// PemMemoryLimit is a memory limit applied specifically to PEM pods.
	PemMemoryLimit string `json:"pemMemoryLimit,omitempty"`
	// PemMemoryRequest is a memory request applied specifically
	// to PEM pods. It will automatically use the value of pemMemoryLimit
	// if not specified.
	PemMemoryRequest string `json:"pemMemoryRequest,omitempty"`
	// ClockConverter specifies which routine to use for converting timestamps to a synced reference time.
	ClockConverter ClockConverterType `json:"clockConverter,omitempty"`
	// Pod defines the policy for creating Vizier pods.
	Pod *PodPolicy `json:"pod,omitempty"`
	// Patches defines patches that should be applied to Vizier resources.
	// The key of the patch should be the name of the resource that is patched. The value of the patch is the patch,
	// encoded as a string which follow the "strategic merge patch" rules for K8s.
	Patches map[string]string `json:"patches,omitempty"`
	// DataAccess defines the level of data that may be accesssed when executing a script on the cluster. If none specified,
	// assumes full data access.
	DataAccess DataAccessLevel `json:"dataAccess,omitempty"`
	// DataCollectorParams specifies the set of params for configuring the dataCollector. If no params are specified, defaults are used.
	DataCollectorParams *DataCollectorParams `json:"dataCollectorParams,omitempty"`
	// LeadershipElectionParams specifies configurable values for the K8s leaderships elections which Vizier uses manage pod leadership.
	LeadershipElectionParams *LeadershipElectionParams `json:"leadershipElectionParams,omitempty"`
	// Registry specifies the image registry to use rather than Pixie's default registry (gcr.io). We expect any forward slashes in
	// Pixie's image paths are replaced with a "-". For example: "gcr.io/pixie-oss/pixie-dev/vizier/metadata_server_image:latest"
	// should be pushed to "$registry/gcr.io-pixie-oss-pixie-dev-vizier-metadata_server_image:latest".
	Registry string `json:"registry,omitempty"`
	// Autopilot should be set if running Pixie on GKE Autopilot.
	Autopilot bool `json:"autopilot,omitempty"`
}

// DataAccessLevel defines the levels of data access that can be used when executing a script on a cluster.
// +kubebuilder:validation:Enum=Full;Restricted
type DataAccessLevel string

const (
	// DataAccessUnknown indicates that the data access level is unspecified.
	DataAccessUnknown DataAccessLevel = ""
	// DataAccessFull provides complete, unrestricted access to all collected data.
	DataAccessFull DataAccessLevel = "Full"
	// DataAccessRestricted restricts users from accessing columns that may contain sensitive data, for example: HTTP response
	// bodies. These columns will be entirely replaced by a redacted string.
	DataAccessRestricted DataAccessLevel = "Restricted"
	// DataAccessPIIRestricted does a best effort redaction of PII. Current PII types include: IP addresses, email addresses, MAC addresses, credit card numbers, and IMEI numbers.
	// Note that the best effort redaction is not perfect and as such if security and privacy are of the utmost concern, one should use DataAccessRestricted.
	DataAccessPIIRestricted DataAccessLevel = "PIIRestricted"
)

// ClockConverterType defines which clock conversion routine to use for converting timestamps to a synced reference time.
// +kubebuilder:validation:Enum=default;grpc
type ClockConverterType string

const (
	// ClockConverterDefault specifies using the default clock conversion routine.
	ClockConverterDefault ClockConverterType = "default"
	// ClockConverterGrpc specifies using the grpc clocksync integration to convert to a synced reference time.
	ClockConverterGrpc ClockConverterType = "grpc"
)

// VizierStatus defines the observed state of Vizier
type VizierStatus struct {
	// Version is the actual version of the Vizier instance.
	Version string `json:"version,omitempty"`
	// VizierPhase is a high-level summary of where the Vizier is in its lifecycle.
	VizierPhase VizierPhase `json:"vizierPhase,omitempty"`
	// VizierReason is a short, machine understandable string that gives the reason
	// for the transition into the Vizier's current status.
	VizierReason string `json:"vizierReason,omitempty"`
	// ReconciliationPhase describes the state the Reconciler is in for this Vizier. See the
	// documentation above the ReconciliationPhase type for more information.
	ReconciliationPhase ReconciliationPhase `json:"reconciliationPhase,omitempty"`
	// LastReconciliationPhaseTime is the last time that the ReconciliationPhase changed.
	LastReconciliationPhaseTime *metav1.Time `json:"lastReconciliationPhaseTime,omitempty"`
	// Message is a human-readable message with details about why the Vizier is in this condition.
	Message string `json:"message,omitempty"`
	// SentryDSN is key for Viziers that is used to send errors and stacktraces to Sentry.
	SentryDSN string `json:"sentryDSN,omitempty"`
	// A checksum of the last reconciled Vizier spec. If this checksum does not match the checksum
	// of the current vizier spec, reconciliation should be performed.
	Checksum []byte `json:"checksum,omitempty"`
	// OperatorVersion is the actual version of the Operator instance.
	OperatorVersion string `json:"operatorVersion,omitempty"`
}

// VizierPhase is a high-level summary of where the Vizier is in its lifecycle.
type VizierPhase string

const (
	// VizierPhaseNone indicates that the vizier phase is unknown.
	VizierPhaseNone VizierPhase = ""

	// VizierPhaseDisconnected indicates that the vizier has been unable to contact and register with Pixie Cloud.
	VizierPhaseDisconnected VizierPhase = "Disconnected"
	// VizierPhaseHealthy indicates that the vizier is fully functioning and queryable.
	VizierPhaseHealthy VizierPhase = "Healthy"
	// VizierPhaseUpdating indicates that the vizier is in the process of creating or updating.
	VizierPhaseUpdating VizierPhase = "Updating"
	// VizierPhaseUnhealthy indicates that the vizier is not in a healthy state and is unqueryable.
	VizierPhaseUnhealthy VizierPhase = "Unhealthy"
	// VizierPhaseDegraded indicates that the vizier is in a queryable state, but data may be missing.
	VizierPhaseDegraded VizierPhase = "Degraded"
)

// ReconciliationPhase is the state the Reconciler has reached while managing this
// vizier. When the Reconciler creates a Vizier, the Reconciler sets this value to `Updating`.
// When successful, the Reconciler moves to a `Ready` phase. If unsuccessful,
// will move from `Updating` to `Failed`. When the Reconciler updates the Vizier
// again, the phase will be set to `Updating`.
type ReconciliationPhase string

const (
	// ReconciliationPhaseNone indicates that the Reconciler does not know the Vizier's Reconcilliation state.
	ReconciliationPhaseNone ReconciliationPhase = ""
	// ReconciliationPhaseReady indicates that the Reconciler has finished updating to the desired Vizier version.
	ReconciliationPhaseReady ReconciliationPhase = "Ready"
	// ReconciliationPhaseUpdating indicates that the Reconciler is currently updating this Vizier.
	ReconciliationPhaseUpdating ReconciliationPhase = "Updating"
	// ReconciliationPhaseFailed indicates that the Reconciler failed to apply the desired Vizier version.
	ReconciliationPhaseFailed ReconciliationPhase = "Failed"
)

// PodPolicy defines the policy for creating Vizier pods.
type PodPolicy struct {
	// Labels specifies the labels to attach to pods the operator creates.
	Labels map[string]string `json:"labels,omitempty"`
	// Annotations specifies the annotations to attach to pods the operator creates.
	Annotations map[string]string `json:"annotations,omitempty"`
	// Resources is the resource requirements for a container.
	// This field cannot be updated once the cluster is created.
	Resources v1.ResourceRequirements `json:"resources,omitempty"`
	// NodeSelector is a selector which must be true for the pod to fit on a node.
	// Selector which must match a node's labels for the pod to be scheduled on that node.
	// More info: https://kubernetes.io/docs/concepts/configuration/assign-pod-node/
	// This field cannot be updated once the cluster is created.
	NodeSelector map[string]string `json:"nodeSelector,omitempty"`
	// Tolerations allows scheduling pods on nodes with matching taints.
	// More info: https://kubernetes.io/docs/concepts/scheduling-eviction/taint-and-toleration/:
	// This field cannot be updated once the cluster is created.
	Tolerations []v1.Toleration `json:"tolerations,omitempty"`
	// The securityContext which should be set on non-privileged pods. All pods which require privileged permissions
	// will still require a privileged securityContext.
	SecurityContext *PodSecurityContext `json:"securityContext,omitempty"`
}

// PodSecurityContext describes the desired security context for non-privileged pods. This may be required for some
// cases with more restrictive PodSecurityAdmissions.
type PodSecurityContext struct {
	// Whether a securityContext should be set on the pod. In cases where no PSPs are applied to the cluster, this is
	// not necessary.
	Enabled bool `json:"enabled,omitempty"`
	// A special supplemental group that applies to all containers in a pod.
	FSGroup int64 `json:"fsGroup,omitempty"`
	// The UID to run the entrypoint of the container process.
	RunAsUser int64 `json:"runAsUser,omitempty"`
	// The GID to run the entrypoint of the container process.
	RunAsGroup int64 `json:"runAsGroup,omitempty"`
}

// DataCollectorParams specifies internal data collector configurations.
type DataCollectorParams struct {
	// DatastreamBufferSize is the data buffer size per connection.
	// Default size is 1 Mbyte. For high-throughput applications, try increasing this number if experiencing data loss.
	DatastreamBufferSize uint32 `json:"datastreamBufferSize,omitempty"`
	// DatastreamBufferSpikeSize is the maximum temporary size of a data stream buffer before processing.
	DatastreamBufferSpikeSize uint32 `json:"datastreamBufferSpikeSize,omitempty"`
	// This contains custom flags that should be passed to the PEM via environment variables.
	CustomPEMFlags map[string]string `json:"customPEMFlags,omitempty"`
}

// LeadershipElectionParams specifies configurable values for the K8s leaderships elections which Vizier uses manage pod leadership.
type LeadershipElectionParams struct {
	// ElectionPeriodMs defines how frequently Vizier attempts to run a K8s leader election, in milliseconds. The period
	// also determines how long Vizier waits for a leader election response back from the K8s API. If the K8s API is
	// slow to respond, consider increasing this number.
	ElectionPeriodMs int64 `json:"electionPeriodMs,omitempty"`
}

// Vizier is the Schema for the viziers API
// +genclient
// +genclient:noStatus
// +kubebuilder:object:root=true
// +kubebuilder:subresource:status
type Vizier struct {
	metav1.TypeMeta   `json:",inline"`
	metav1.ObjectMeta `json:"metadata,omitempty"`

	Spec   VizierSpec   `json:"spec,omitempty"`
	Status VizierStatus `json:"status,omitempty"`
}

// SetReconciliationPhase updates the Vizier status with the given ReconciliationPhase.
func (vz *Vizier) SetReconciliationPhase(rp ReconciliationPhase) {
	vz.Status.ReconciliationPhase = rp
	timeNow := metav1.Now()
	vz.Status.LastReconciliationPhaseTime = &timeNow
}

// SetStatus updates the Vizier status with the given Reason.
func (vz *Vizier) SetStatus(reason status.VizierReason) {
	vz.Status.VizierPhase = ReasonToPhase(reason)
	vz.Status.VizierReason = string(reason)
	vz.Status.Message = reason.GetMessage()
}

// ReasonToPhase converts the Reason into the relevant Phase.
func ReasonToPhase(reason status.VizierReason) VizierPhase {
	switch reason {
	case "":
		return VizierPhaseHealthy
	case status.CloudConnectorMissing:
		return VizierPhaseDisconnected
	case status.PEMsSomeInsufficientMemory, status.KernelVersionsIncompatible, status.PEMsHighFailureRate:
		return VizierPhaseDegraded
	default:
		return VizierPhaseUnhealthy
	}
}

// VizierList contains a list of Vizier
// +kubebuilder:object:root=true
type VizierList struct {
	metav1.TypeMeta `json:",inline"`
	metav1.ListMeta `json:"metadata,omitempty"`
	Items           []Vizier `json:"items"`
}
