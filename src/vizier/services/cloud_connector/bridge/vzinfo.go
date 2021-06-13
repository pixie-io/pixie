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
	"k8s.io/client-go/kubernetes"
	"k8s.io/client-go/kubernetes/scheme"

	// Blank import necessary for kubeConfig to work.
	"k8s.io/client-go/discovery"
	_ "k8s.io/client-go/plugin/pkg/client/auth/gcp"
	"k8s.io/client-go/rest"
	"k8s.io/client-go/tools/cache"

	"px.dev/pixie/src/api/proto/vizierpb"
	"px.dev/pixie/src/operator/api/v1alpha1"
	"px.dev/pixie/src/shared/cvmsgspb"
	version "px.dev/pixie/src/shared/goversion"
	protoutils "px.dev/pixie/src/shared/k8s"
	"px.dev/pixie/src/shared/k8s/metadatapb"
)

const k8sStateUpdatePeriod = 10 * time.Second

const privateImageRepo = "gcr.io/pixie-oss/pixie-dev"
const publicImageRepo = "gcr.io/pixie-oss/pixie-prod"

// K8sJobHandler manages k8s jobs.
type K8sJobHandler interface {
	CleanupCronJob(string, time.Duration, chan bool)
}

// K8sVizierInfo is responsible for fetching Vizier information through K8s.
type K8sVizierInfo struct {
	ns                   string
	clientset            *kubernetes.Clientset
	vzClient             *v1alpha1.VizierClient
	clusterVersion       string
	clusterName          string
	currentPodStatus     map[string]*cvmsgspb.PodStatus
	k8sStateLastUpdated  time.Time
	numNodes             int32
	numInstrumentedNodes int32
	mu                   sync.Mutex
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

	vzCrdClient, err := v1alpha1.NewVizierClient(kubeConfig)
	if err != nil {
		log.WithError(err).Error("Failed to initialize vizier CRD client")
		return nil, err
	}

	clusterVersion := ""

	discoveryClient, err := discovery.NewDiscoveryClientForConfig(kubeConfig)
	if err != nil {
		log.WithError(err).Error("Failed to get discovery client from kubeConfig")
	}

	version, err := discoveryClient.ServerVersion()
	if err != nil {
		log.WithError(err).Error("Failed to get server version from discovery client")
	} else {
		clusterVersion = version.GitVersion
	}

	vzInfo := &K8sVizierInfo{
		ns:             ns,
		clientset:      clientset,
		vzClient:       vzCrdClient,
		clusterVersion: clusterVersion,
		clusterName:    clusterName,
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
		ClusterUID:     clusterUID,
		ClusterName:    v.clusterName,
		ClusterVersion: v.clusterVersion,
		VizierVersion:  version.GetVersion().ToString(),
	}, nil
}

// GetAddress gets the external address of Vizier's proxy service.
func (v *K8sVizierInfo) GetAddress() (string, int32, error) {
	proxySvc, err := v.clientset.CoreV1().Services(v.ns).Get(context.Background(), "vizier-proxy-service", metav1.GetOptions{})
	if err != nil {
		return "", int32(0), err
	}

	ip := ""
	port := int32(0)

	switch proxySvc.Spec.Type {
	case corev1.ServiceTypeNodePort:
		nodesList, err := v.clientset.CoreV1().Nodes().List(context.Background(), metav1.ListOptions{})
		if err != nil {
			return "", int32(0), err
		}
		if len(nodesList.Items) == 0 {
			return "", int32(0), errors.New("Could not find node for NodePort")
		}

		// Just select the first node for now.
		// Don't want to randomly pick, because it'll affect DNS.
		nodeAddrs := nodesList.Items[0].Status.Addresses

		for _, nodeAddr := range nodeAddrs {
			if nodeAddr.Type == corev1.NodeInternalIP {
				ip = nodeAddr.Address
			}
		}
		if ip == "" {
			return "", int32(0), errors.New("Could not determine IP address of node for NodePort")
		}

		for _, portSpec := range proxySvc.Spec.Ports {
			if portSpec.Name == "tcp-https" {
				port = portSpec.NodePort
			}
		}
		if port <= 0 {
			return "", int32(0), errors.New("Could not determine port for vizier service")
		}
	case corev1.ServiceTypeLoadBalancer:
		// It's possible to have more than one external IP. Just select the first one for now.
		if len(proxySvc.Status.LoadBalancer.Ingress) == 0 {
			return "", int32(0), errors.New("Proxy service has no external IPs")
		}
		ip = proxySvc.Status.LoadBalancer.Ingress[0].IP

		for _, portSpec := range proxySvc.Spec.Ports {
			if portSpec.Name == "tcp-https" {
				port = portSpec.Port
			}
		}
		if port <= 0 {
			return "", int32(0), errors.New("Could not determine port for vizier service")
		}
	default:
		return "", int32(0), errors.New("Unexpected service type")
	}

	externalAddr := ip

	return externalAddr, port, nil
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
	podPb, err := protoutils.PodToProto(p)
	if err != nil {
		return nil, err
	}

	podStatus := &vizierpb.VizierPodStatus{
		Name:      podPb.Metadata.Name,
		CreatedAt: podPb.Metadata.CreationTimestampNS,
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
			})
		}
	}

	return podStatus, nil
}

// GetVizierPods gets the Vizier pods and their statuses.
func (v *K8sVizierInfo) GetVizierPods() ([]*vizierpb.VizierPodStatus, []*vizierpb.VizierPodStatus, error) {
	// Get only control-plane pods.
	rawControlPodsList, err := v.clientset.CoreV1().Pods(v.ns).List(context.Background(), metav1.ListOptions{
		LabelSelector: "plane=control",
	})
	if err != nil {
		return nil, nil, err
	}
	// Get only data-plane pods.
	rawDataPodsList, err := v.clientset.CoreV1().Pods(v.ns).List(context.Background(), metav1.ListOptions{
		LabelSelector: "plane=data",
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

// UpdateK8sState gets the relevant state of the cluster, such as pod statuses, at the current moment in time.
func (v *K8sVizierInfo) UpdateK8sState() {
	nodesList, err := v.clientset.CoreV1().Nodes().List(context.Background(), metav1.ListOptions{})
	if err != nil {
		return
	}
	// Get only control-plane pods.
	cpPodsList, err := v.clientset.CoreV1().Pods(v.ns).List(context.Background(), metav1.ListOptions{
		LabelSelector: "plane=control",
	})
	if err != nil {
		return
	}
	// Get only pem.
	pemPodsList, err := v.clientset.CoreV1().Pods(v.ns).List(context.Background(), metav1.ListOptions{
		LabelSelector: "name=vizier-pem",
	})
	if err != nil {
		return
	}
	// Get the count of healthy PEMs.
	healthyPemCount := 0
	for _, p := range pemPodsList.Items {
		podPb, err := protoutils.PodToProto(&p)
		if err != nil {
			return
		}

		if podPb.Status != nil && podPb.Status.Phase == metadatapb.RUNNING {
			healthyPemCount++
		}
	}

	now := time.Now()

	podMap := make(map[string]*cvmsgspb.PodStatus)
	for _, p := range cpPodsList.Items {
		podPb, err := protoutils.PodToProto(&p)
		if err != nil {
			return
		}

		status := metadatapb.PHASE_UNKNOWN
		msg := ""
		containers := make([]*cvmsgspb.ContainerStatus, 0)
		if podPb.Status != nil {
			status = podPb.Status.Phase
			msg = podPb.Status.Reason
			for _, c := range podPb.Status.ContainerStatuses {
				containers = append(containers, &cvmsgspb.ContainerStatus{
					Name:      c.Name,
					Message:   c.Message,
					Reason:    c.Reason,
					State:     c.ContainerState,
					CreatedAt: nanosToTimestampProto(c.StartTimestampNS),
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
			log.WithError(err).Info("Error getting K8s events")
		}

		s := &cvmsgspb.PodStatus{
			Name:          name,
			Status:        status,
			StatusMessage: msg,
			Containers:    containers,
			CreatedAt:     nanosToTimestampProto(podPb.Metadata.CreationTimestampNS),
			Events:        events,
		}
		podMap[name] = s
	}

	v.mu.Lock()
	defer v.mu.Unlock()

	v.currentPodStatus = podMap
	v.k8sStateLastUpdated = now
	v.numNodes = int32(len(nodesList.Items))
	v.numInstrumentedNodes = int32(healthyPemCount)
}

// GetPodStatuses gets the pod statuses and the last time they were updated.
func (v *K8sVizierInfo) GetK8sState() (map[string]*cvmsgspb.PodStatus, int32, int32, time.Time) {
	v.mu.Lock()
	defer v.mu.Unlock()

	return v.currentPodStatus, v.numNodes, v.numInstrumentedNodes, v.k8sStateLastUpdated
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

			repoPath := publicImageRepo
			if len(tagVers.Pre) > 0 {
				repoPath = privateImageRepo
			}
			imgPath = repoPath + imgPath[strings.Index(imgPath, "/vizier/"):]

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

// GetClusterID fetches the cluster ID from the cluster secrets.
func (v *K8sVizierInfo) GetClusterID() (string, error) {
	s, err := v.clientset.CoreV1().Secrets(v.ns).Get(context.Background(), "pl-cluster-secrets", metav1.GetOptions{})
	if err != nil {
		return "", err
	}
	if id, ok := s.Data["cluster-id"]; ok {
		return string(id), nil
	}
	return "", errors.New("cluster-id not found in pl-cluster-secrets")
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

// UpdateCRDVizierVersion updates the version of the Vizier CRD in the namespace.
// Returns an error if the CRD was not found, or if an error occurred while
// updating the CRD.
func (v *K8sVizierInfo) UpdateCRDVizierVersion(version string) error {
	viziers, err := v.vzClient.List(context.Background(), v.ns, metav1.ListOptions{})
	if err != nil {
		return err
	}
	if len(viziers.Items) > 0 {
		vz := viziers.Items[0]
		vz.Spec.Version = version
		_, err = v.vzClient.Update(context.Background(), &vz, v.ns, metav1.UpdateOptions{})
		if err != nil {
			return err
		}
		return nil
	}
	return errors.New("No vizier CRD found")
}
