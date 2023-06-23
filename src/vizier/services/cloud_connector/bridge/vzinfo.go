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

package bridge

import (
	"context"
	"errors"
	"fmt"
	"sort"
	"strings"
	"sync"
	"time"

	"github.com/blang/semver"
	"github.com/gogo/protobuf/types"
	log "github.com/sirupsen/logrus"
	batchv1 "k8s.io/api/batch/v1"
	corev1 "k8s.io/api/core/v1"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/apimachinery/pkg/fields"
	"k8s.io/client-go/discovery"
	"k8s.io/client-go/kubernetes"
	"k8s.io/client-go/kubernetes/scheme"
	"k8s.io/client-go/rest"
	"k8s.io/client-go/tools/cache"

	"px.dev/pixie/src/api/proto/vizierpb"
	"px.dev/pixie/src/operator/apis/px.dev/v1alpha1"
	"px.dev/pixie/src/operator/client/versioned"
	"px.dev/pixie/src/shared/cvmsgspb"
	version "px.dev/pixie/src/shared/goversion"
	protoutils "px.dev/pixie/src/shared/k8s"
	"px.dev/pixie/src/shared/k8s/metadatapb"
	"px.dev/pixie/src/utils/shared/k8s"
)

const k8sStateUpdatePeriod = 10 * time.Second

const privateImageRepo = "gcr.io/pixie-oss/pixie-dev"
const publicImageRepo = "gcr.io/pixie-oss/pixie-prod"

// K8sState describes the Kubernetes state of the Vizier instance.
type K8sState struct {
	// Pod statuses for Vizier control plane pods.
	ControlPlanePodStatuses map[string]*cvmsgspb.PodStatus
	// Pod statuses for a sample (10) of unhealthy Vizier pods.
	UnhealthyDataPlanePodStatuses map[string]*cvmsgspb.PodStatus
	// The current K8s version of Vizier.
	K8sClusterVersion string
	// The number of nodes on the cluster.
	NumNodes int32
	// The number of nodes on the cluster that are running a PEM.
	NumInstrumentedNodes int32
	// The last time this information was updated.
	LastUpdated time.Time
}

// K8sJobHandler manages k8s jobs.
type K8sJobHandler interface {
	CleanupCronJob(string, time.Duration, chan bool)
}

// K8sVizierInfo is responsible for fetching Vizier information through K8s.
type K8sVizierInfo struct {
	ns                            string
	clientset                     *kubernetes.Clientset
	vzClient                      *versioned.Clientset
	clusterVersion                string
	clusterName                   string
	controlPlanePodStatuses       map[string]*cvmsgspb.PodStatus
	unhealthyDataPlanePodStatuses map[string]*cvmsgspb.PodStatus
	k8sStateLastUpdated           time.Time
	numNodes                      int32
	numInstrumentedNodes          int32
	mu                            sync.Mutex
}

func getK8sVersion() (string, error) {
	kubeConfig, err := rest.InClusterConfig()
	if err != nil {
		return "", err
	}

	discoveryClient, err := discovery.NewDiscoveryClientForConfig(kubeConfig)
	if err != nil {
		return "", err
	}

	version, err := discoveryClient.ServerVersion()
	if err != nil {
		return "", err
	}
	return version.GitVersion, nil
}

// NewK8sVizierInfo creates a new K8sVizierInfo.
func NewK8sVizierInfo(clusterName, ns string) (*K8sVizierInfo, error) {
	// There is a specific config for services running in the cluster.
	kubeConfig, err := rest.InClusterConfig()
	if err != nil {
		return nil, err
	}

	// Create k8s client.
	clientset, err := kubernetes.NewForConfig(kubeConfig)
	if err != nil {
		return nil, err
	}

	vzCrdClient, err := versioned.NewForConfig(kubeConfig)
	if err != nil {
		log.WithError(err).Error("Failed to initialize vizier CRD client")
		return nil, err
	}

	vzInfo := &K8sVizierInfo{
		ns:          ns,
		clientset:   clientset,
		vzClient:    vzCrdClient,
		clusterName: clusterName,
	}

	go func() {
		t := time.NewTicker(k8sStateUpdatePeriod)
		defer t.Stop()
		for range t.C {
			vzInfo.UpdateK8sState()
		}
	}()

	return vzInfo, nil
}

// GetVizierClusterInfo gets the K8s cluster info for the current running vizier.
func (v *K8sVizierInfo) GetVizierClusterInfo() (*cvmsgspb.VizierClusterInfo, error) {
	clusterUID, err := v.GetClusterUID()
	if err != nil {
		return nil, err
	}
	return &cvmsgspb.VizierClusterInfo{
		ClusterUID:    clusterUID,
		ClusterName:   v.clusterName,
		VizierVersion: version.GetVersion().ToString(),
	}, nil
}

// GetClusterUID gets UID for the cluster, represented by the kube-system namespace UID.
func (v *K8sVizierInfo) GetClusterUID() (string, error) {
	ksNS, err := v.clientset.CoreV1().Namespaces().Get(context.Background(), "kube-system", metav1.GetOptions{})
	if err != nil {
		return "", err
	}
	return string(ksNS.UID), nil
}

const nanosPerSecond = int64(1000 * 1000 * 1000)

func nanosToTimestampProto(nanos int64) *types.Timestamp {
	seconds := nanos / nanosPerSecond
	remainderNanos := int32(nanos % nanosPerSecond)
	return &types.Timestamp{
		Seconds: seconds,
		Nanos:   remainderNanos,
	}
}

// GetVizierPodLogs gets the k8s logs for the Vizier pod with the given name.
func (v *K8sVizierInfo) GetVizierPodLogs(podName string, previous bool, container string) (string, error) {
	resp := v.clientset.CoreV1().Pods(v.ns).GetLogs(podName, &corev1.PodLogOptions{Previous: previous, Container: container}).Do(context.Background())
	rawResp, err := resp.Raw()
	if err != nil {
		return "", err
	}
	return string(rawResp), nil
}

func convertPodPhase(p metadatapb.PodPhase) vizierpb.PodPhase {
	switch p {
	case metadatapb.PENDING:
		return vizierpb.PENDING
	case metadatapb.RUNNING:
		return vizierpb.RUNNING
	case metadatapb.SUCCEEDED:
		return vizierpb.SUCCEEDED
	case metadatapb.FAILED:
		return vizierpb.FAILED
	case metadatapb.PHASE_UNKNOWN:
		return vizierpb.PHASE_UNKNOWN
	default:
		return vizierpb.PHASE_UNKNOWN
	}
}

func convertContainerState(p metadatapb.ContainerState) vizierpb.ContainerState {
	switch p {
	case metadatapb.CONTAINER_STATE_RUNNING:
		return vizierpb.CONTAINER_STATE_RUNNING
	case metadatapb.CONTAINER_STATE_TERMINATED:
		return vizierpb.CONTAINER_STATE_TERMINATED
	case metadatapb.CONTAINER_STATE_WAITING:
		return vizierpb.CONTAINER_STATE_WAITING
	case metadatapb.CONTAINER_STATE_UNKNOWN:
		return vizierpb.CONTAINER_STATE_UNKNOWN
	default:
		return vizierpb.CONTAINER_STATE_UNKNOWN
	}
}

func (v *K8sVizierInfo) toVizierPodStatus(p *corev1.Pod) (*vizierpb.VizierPodStatus, error) {
	podPb := protoutils.PodToProto(p)

	podStatus := &vizierpb.VizierPodStatus{
		Name:         fmt.Sprintf("%s/%s", podPb.Metadata.Namespace, podPb.Metadata.Name),
		CreatedAt:    podPb.Metadata.CreationTimestampNS,
		RestartCount: podPb.Status.RestartCount,
	}

	if podPb.Status != nil {
		podStatus.Phase = convertPodPhase(podPb.Status.Phase)
		podStatus.Message = podPb.Status.Message
		podStatus.Reason = podPb.Status.Reason
		for _, c := range podPb.Status.ContainerStatuses {
			podStatus.ContainerStatuses = append(podStatus.ContainerStatuses, &vizierpb.ContainerStatus{
				Name:             c.Name,
				Message:          c.Message,
				Reason:           c.Reason,
				ContainerState:   convertContainerState(c.ContainerState),
				StartTimestampNS: c.StartTimestampNS,
				RestartCount:     c.RestartCount,
			})
		}
	}

	return podStatus, nil
}

// GetVizierPods gets the Vizier pods and their statuses.
func (v *K8sVizierInfo) GetVizierPods() ([]*vizierpb.VizierPodStatus, []*vizierpb.VizierPodStatus, error) {
	vls := k8s.VizierLabelSelector()
	// Get only control-plane pods.
	vls.MatchLabels["plane"] = "control"
	rawControlPodsList, err := v.clientset.CoreV1().Pods(v.ns).List(context.Background(), metav1.ListOptions{
		LabelSelector: metav1.FormatLabelSelector(&vls),
	})
	if err != nil {
		return nil, nil, err
	}

	vls = k8s.VizierLabelSelector()
	// Get only data-plane pods.
	vls.MatchLabels["plane"] = "data"
	rawDataPodsList, err := v.clientset.CoreV1().Pods(v.ns).List(context.Background(), metav1.ListOptions{
		LabelSelector: metav1.FormatLabelSelector(&vls),
	})

	var controlPods []*vizierpb.VizierPodStatus
	var dataPods []*vizierpb.VizierPodStatus

	for _, rawPod := range rawControlPodsList.Items {
		pod, err := v.toVizierPodStatus(&rawPod)
		if err != nil {
			return nil, nil, err
		}
		controlPods = append(controlPods, pod)
	}

	for _, rawPod := range rawDataPodsList.Items {
		pod, err := v.toVizierPodStatus(&rawPod)
		if err != nil {
			return nil, nil, err
		}
		dataPods = append(dataPods, pod)
	}

	return controlPods, dataPods, err
}

// Convert a list of K8s pod information to our internal (cloud) representation of PodStatus.
func (v *K8sVizierInfo) getPodStatuses(podList []corev1.Pod) (map[string]*cvmsgspb.PodStatus, error) {
	podMap := make(map[string]*cvmsgspb.PodStatus)

	for _, p := range podList {
		podPb := protoutils.PodToProto(&p)

		status := metadatapb.PHASE_UNKNOWN
		msg := ""
		containers := make([]*cvmsgspb.ContainerStatus, 0)

		if podPb.Status != nil {
			status = podPb.Status.Phase
			msg = podPb.Status.Reason
			for _, c := range podPb.Status.ContainerStatuses {
				containers = append(containers, &cvmsgspb.ContainerStatus{
					Name:         c.Name,
					Message:      c.Message,
					Reason:       c.Reason,
					State:        c.ContainerState,
					CreatedAt:    nanosToTimestampProto(c.StartTimestampNS),
					RestartCount: c.RestartCount,
				})
			}
		}
		name := podPb.Metadata.Name
		ns := v.ns
		events := make([]*cvmsgspb.K8SEvent, 0)

		eventsInterface := v.clientset.CoreV1().Events(v.ns)
		selector := eventsInterface.GetFieldSelector(&name, &ns, nil, nil)
		options := metav1.ListOptions{FieldSelector: selector.String()}
		evs, err := eventsInterface.List(context.Background(), options)

		if err == nil {
			// Limit to last 5 events.
			start := len(evs.Items) - 5
			if start < 0 {
				start = 0
			}
			end := len(evs.Items)

			for start < end {
				e := evs.Items[start]
				events = append(events, &cvmsgspb.K8SEvent{
					Message:   e.Message,
					FirstTime: nanosToTimestampProto(e.FirstTimestamp.UnixNano()),
					LastTime:  nanosToTimestampProto(e.LastTimestamp.UnixNano()),
				})
				start++
			}
		} else {
			return nil, err
		}

		s := &cvmsgspb.PodStatus{
			Name:          name,
			Status:        status,
			StatusMessage: msg,
			Containers:    containers,
			CreatedAt:     nanosToTimestampProto(podPb.Metadata.CreationTimestampNS),
			Events:        events,
			RestartCount:  podPb.Status.RestartCount,
		}
		podMap[name] = s
	}
	return podMap, nil
}

func (v *K8sVizierInfo) getControlPlanePodStatuses() (map[string]*cvmsgspb.PodStatus, error) {
	// Get only control-plane pods.
	cpPodsList, err := v.clientset.CoreV1().Pods(v.ns).List(context.Background(), metav1.ListOptions{
		LabelSelector: "plane=control",
	})
	if err != nil {
		return nil, err
	}
	return v.getPodStatuses(cpPodsList.Items)
}

// Capture K8s state related to the data plane (num nodes, num instrumented nodes, unhealthy data plane pods)
func (v *K8sVizierInfo) getDataPlaneState() (int32, int32, map[string]*cvmsgspb.PodStatus, error) {
	nodesList, err := v.clientset.CoreV1().Nodes().List(context.Background(), metav1.ListOptions{})
	if err != nil {
		log.WithError(err).Error("Error fetching nodes")
		return 0, 0, nil, err
	}

	var unhealthyDataPlanePods []corev1.Pod

	kelvinPodsList, err := v.clientset.CoreV1().Pods(v.ns).List(context.Background(), metav1.ListOptions{
		LabelSelector: "name=kelvin",
	})
	if err != nil {
		log.WithError(err).Error("Error fetching Kelvin pods")
		return 0, 0, nil, err
	}
	for _, kelvinPod := range kelvinPodsList.Items {
		if kelvinPod.Status.Phase != corev1.PodRunning {
			unhealthyDataPlanePods = append(unhealthyDataPlanePods, kelvinPod)
		}
	}

	var unhealthyPEMPods []corev1.Pod
	pemPodsList, err := v.clientset.CoreV1().Pods(v.ns).List(context.Background(), metav1.ListOptions{
		LabelSelector: "name=vizier-pem",
	})
	if err != nil {
		log.WithError(err).Error("Error fetching PEM pods")
		return 0, 0, nil, err
	}

	// Get the count of healthy PEMs.
	healthyPemCount := 0
	for _, pemPod := range pemPodsList.Items {
		if pemPod.Status.Phase == corev1.PodRunning {
			healthyPemCount++
		} else {
			unhealthyPEMPods = append(unhealthyPEMPods, pemPod)
		}
	}

	// Sort the unhealthy PEM pods. Get the first N.
	maxUnhealthyDataPlanePods := 10
	sort.Slice(unhealthyPEMPods, func(i, j int) bool {
		return unhealthyPEMPods[i].ObjectMeta.Name < unhealthyPEMPods[j].ObjectMeta.Name
	})
	for i := 0; i < len(unhealthyPEMPods); i++ {
		if len(unhealthyDataPlanePods) <= maxUnhealthyDataPlanePods {
			break
		}
		unhealthyDataPlanePods = append(unhealthyDataPlanePods, unhealthyPEMPods[i])
	}

	unhealthyDataPlanePodStatuses, err := v.getPodStatuses(unhealthyDataPlanePods)
	if err != nil {
		return 0, 0, nil, err
	}
	return int32(len(nodesList.Items)), int32(healthyPemCount), unhealthyDataPlanePodStatuses, nil
}

// UpdateK8sState gets the relevant state of the cluster, such as pod statuses, at the current moment in time.
func (v *K8sVizierInfo) UpdateK8sState() {
	controlPlanePods, err := v.getControlPlanePodStatuses()
	if err != nil {
		log.WithError(err).Error("Error fetching control plane pod statuses")
		return
	}

	numNodes, numInstrumentedNodes, unhealthyDataPlanePods, err := v.getDataPlaneState()
	if err != nil {
		log.WithError(err).Error("Error fetching data plane pod information")
		return
	}

	clusterVersion, err := getK8sVersion()
	if err != nil {
		log.WithError(err).Error("Failed to get Kubernetes version for cluster")
		return
	}

	now := time.Now()
	v.mu.Lock()
	defer v.mu.Unlock()

	v.k8sStateLastUpdated = now
	v.controlPlanePodStatuses = controlPlanePods
	v.unhealthyDataPlanePodStatuses = unhealthyDataPlanePods
	v.numNodes = numNodes
	v.numInstrumentedNodes = numInstrumentedNodes
	v.clusterVersion = clusterVersion
}

// Function to copy pod statuses since maps are a reference type and we return
// a map to the downstream consumers of K8sState.
func copyPodStatus(podStatuses map[string]*cvmsgspb.PodStatus) map[string]*cvmsgspb.PodStatus {
	if podStatuses == nil {
		return nil
	}
	clone := make(map[string]*cvmsgspb.PodStatus)
	for k, v := range podStatuses {
		clone[k] = v
	}
	return clone
}

// GetK8sState gets the pod statuses and the last time they were updated.
func (v *K8sVizierInfo) GetK8sState() *K8sState {
	v.mu.Lock()
	defer v.mu.Unlock()

	return &K8sState{
		ControlPlanePodStatuses:       copyPodStatus(v.controlPlanePodStatuses),
		UnhealthyDataPlanePodStatuses: copyPodStatus(v.unhealthyDataPlanePodStatuses),
		NumNodes:                      v.numNodes,
		NumInstrumentedNodes:          v.numInstrumentedNodes,
		LastUpdated:                   v.k8sStateLastUpdated,
		K8sClusterVersion:             v.clusterVersion,
	}
}

// ParseJobYAML parses the yaml string into a k8s job and applies the image tag and env subtitutions.
func (v *K8sVizierInfo) ParseJobYAML(yamlStr string, imageTag map[string]string, envSubtitutions map[string]string) (*batchv1.Job, error) {
	decode := scheme.Codecs.UniversalDeserializer().Decode
	obj, _, err := decode([]byte(yamlStr), nil, nil)
	if err != nil {
		return nil, err
	}

	job, ok := obj.(*batchv1.Job)
	if !ok {
		return nil, errors.New("YAML could not be decoded to job")
	}

	// Add proper image tag and env substitutions.
	for i, c := range job.Spec.Template.Spec.Containers {
		if val, ok := imageTag[c.Name]; ok {
			imgTag := strings.Split(job.Spec.Template.Spec.Containers[i].Image, ":")
			imgPath := imgTag[0]

			tagVers := semver.MustParse(val)
			if len(tagVers.Pre) > 0 && strings.HasPrefix(imgPath, publicImageRepo) {
				imgPath = strings.Replace(imgPath, publicImageRepo, privateImageRepo, 1)
			}

			job.Spec.Template.Spec.Containers[i].Image = imgPath + ":" + val
		}
		for j, e := range c.Env {
			if val, ok := envSubtitutions[e.Name]; ok {
				job.Spec.Template.Spec.Containers[i].Env[j].Value = val
			}
		}
	}

	return job, nil
}

// LaunchJob starts the specified job.
func (v *K8sVizierInfo) LaunchJob(j *batchv1.Job) (*batchv1.Job, error) {
	job, err := v.clientset.BatchV1().Jobs(v.ns).Create(context.Background(), j, metav1.CreateOptions{})
	if err != nil {
		return nil, err
	}

	return job, nil
}

// CreateSecret creates the K8s secret.
func (v *K8sVizierInfo) CreateSecret(name string, literals map[string]string) error {
	// Attempt to delete the secret first, if it already exists.
	err := v.clientset.CoreV1().Secrets(v.ns).Delete(context.Background(), name, metav1.DeleteOptions{})
	if err != nil {
		log.WithError(err).Info("Failed to delete secret, possibly because it may not exist")
	}

	secret := &corev1.Secret{}
	secret.SetGroupVersionKind(corev1.SchemeGroupVersion.WithKind("Secret"))

	secret.Name = name
	secret.Data = map[string][]byte{}

	for k, v := range literals {
		secret.Data[k] = []byte(v)
	}

	_, err = v.clientset.CoreV1().Secrets(v.ns).Create(context.Background(), secret, metav1.CreateOptions{})
	if err != nil {
		return err
	}
	return nil
}

// DeleteJob deletes the job with the specified name.
func (v *K8sVizierInfo) DeleteJob(name string) error {
	policy := metav1.DeletePropagationBackground

	return v.clientset.BatchV1().Jobs(v.ns).Delete(context.Background(), name, metav1.DeleteOptions{
		PropagationPolicy: &policy,
	})
}

// GetJob gets the job with the specified name.
func (v *K8sVizierInfo) GetJob(name string) (*batchv1.Job, error) {
	return v.clientset.BatchV1().Jobs(v.ns).Get(context.Background(), name, metav1.GetOptions{})
}

// CleanupCronJob periodically cleans up any completed jobs that were run by the specified cronjob.
func (v *K8sVizierInfo) CleanupCronJob(cronJob string, duration time.Duration, quitCh chan bool) {
	policy := metav1.DeletePropagationBackground
	ticker := time.NewTicker(duration)
	defer ticker.Stop()
	for {
		select {
		case <-ticker.C:
			jobs, err := v.clientset.BatchV1().Jobs(v.ns).List(context.Background(), metav1.ListOptions{})
			if err != nil {
				log.WithError(err).Error("Could not list jobs")
				continue
			}
			for _, j := range jobs.Items {
				if len(j.ObjectMeta.OwnerReferences) > 0 && j.ObjectMeta.OwnerReferences[0].Name == cronJob && j.Status.Succeeded == 1 {
					err = v.clientset.BatchV1().Jobs(v.ns).Delete(context.Background(), j.ObjectMeta.Name, metav1.DeleteOptions{
						PropagationPolicy: &policy,
					})
					if err != nil {
						log.WithError(err).Error("Could not delete job")
					}
				}
			}
		case <-quitCh:
			return
		}
	}
}

// WaitForJobCompletion waits for the job with given name to complete.
func (v *K8sVizierInfo) WaitForJobCompletion(name string) (bool, error) {
	watcher := cache.NewListWatchFromClient(v.clientset.BatchV1().RESTClient(), "jobs", v.ns, fields.OneTermEqualSelector("metadata.name", name))

	w, err := watcher.Watch(metav1.ListOptions{})
	if err != nil {
		return false, err
	}

	for c := range w.ResultChan() {
		o, ok := c.Object.(*batchv1.Job)
		if ok {
			if !o.Status.CompletionTime.IsZero() {
				return true, nil // Job has completed.
			}
			if o.Status.Failed > 0 {
				return false, nil
			}
		}
	}
	return true, nil
}

// UpdateClusterID updates the cluster ID in the cluster secrets.
func (v *K8sVizierInfo) UpdateClusterID(id string) error {
	s, err := v.clientset.CoreV1().Secrets(v.ns).Get(context.Background(), "pl-cluster-secrets", metav1.GetOptions{})
	if err != nil {
		return err
	}
	s.Data["cluster-id"] = []byte(id)

	_, err = v.clientset.CoreV1().Secrets(v.ns).Update(context.Background(), s, metav1.UpdateOptions{})
	return err
}

// UpdateClusterName updates the cluster ID in the cluster secrets.
func (v *K8sVizierInfo) UpdateClusterName(id string) error {
	s, err := v.clientset.CoreV1().Secrets(v.ns).Get(context.Background(), "pl-cluster-secrets", metav1.GetOptions{})
	if err != nil {
		return err
	}
	s.Data["cluster-name"] = []byte(id)

	_, err = v.clientset.CoreV1().Secrets(v.ns).Update(context.Background(), s, metav1.UpdateOptions{})
	return err
}

// UpdateClusterIDAnnotation updates the `cluster-id` annotation for the cloudconnector.
func (v *K8sVizierInfo) UpdateClusterIDAnnotation(id string) error {
	ccPodsList, err := v.clientset.CoreV1().Pods(v.ns).List(context.Background(), metav1.ListOptions{
		LabelSelector: "name=vizier-cloud-connector",
	})
	if err != nil {
		return err
	}

	// No cloud-connector pods were found. This should never happen.
	if len(ccPodsList.Items) == 0 {
		return nil
	}

	ccPod := ccPodsList.Items[0]
	if ccPod.Annotations == nil {
		ccPod.Annotations = make(map[string]string)
	}
	ccPod.Annotations["cluster-id"] = id
	if ccPod.Labels == nil {
		ccPod.Labels = make(map[string]string)
	}
	ccPod.Labels["cluster-id"] = id

	_, err = v.clientset.CoreV1().Pods(v.ns).Update(context.Background(), &ccPod, metav1.UpdateOptions{})
	return err
}

// GetVizierCRD gets the Vizier CRD for the running Vizier, if running using an operator.
func (v *K8sVizierInfo) GetVizierCRD() (*v1alpha1.Vizier, error) {
	viziers, err := v.vzClient.PxV1alpha1().Viziers(v.ns).List(context.Background(), metav1.ListOptions{})
	if err != nil {
		return nil, err
	}
	if len(viziers.Items) > 0 {
		return &viziers.Items[0], nil
	}
	return nil, errors.New("No vizier CRD found")
}

// UpdateCRDVizierVersion updates the version of the Vizier CRD in the namespace.
// Returns an error if the CRD was not found, or if an error occurred while
// updating the CRD. Returns whether or not an update was actually initiated.
// This is used to determine whether the vizier should actually be in "UPDATING"
// status, in the case of falsely initated update requests. This will be fixed
// as we move to having the operator fully manage vizier statuses.
func (v *K8sVizierInfo) UpdateCRDVizierVersion(version string) (bool, error) {
	vz, err := v.GetVizierCRD()
	if err != nil {
		return false, err
	}

	if vz.Spec.Version == version {
		return false, nil
	}

	vz.Spec.Version = version
	_, err = v.vzClient.PxV1alpha1().Viziers(v.ns).Update(context.Background(), vz, metav1.UpdateOptions{})
	if err != nil {
		return false, err
	}
	return true, nil
}
