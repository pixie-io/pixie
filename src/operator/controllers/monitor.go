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
	"crypto/tls"
	"fmt"
	"io"
	"net/http"
	"regexp"
	"strings"
	"sync"
	"time"

	"github.com/blang/semver"
	log "github.com/sirupsen/logrus"
	"google.golang.org/grpc"
	v1 "k8s.io/api/core/v1"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/apimachinery/pkg/fields"
	"k8s.io/apimachinery/pkg/types"
	"k8s.io/client-go/kubernetes"
	"k8s.io/client-go/tools/cache"
	"k8s.io/client-go/tools/watch"
	"sigs.k8s.io/controller-runtime/pkg/client"

	// Blank import necessary for kubeConfig to work.
	_ "k8s.io/client-go/plugin/pkg/client/auth/gcp"

	"px.dev/pixie/src/api/proto/cloudpb"
	pixiev1alpha1 "px.dev/pixie/src/operator/api/v1alpha1"
	"px.dev/pixie/src/shared/status"
	"px.dev/pixie/src/utils/shared/k8s"
)

const (
	// The name label of the cloud-conn pod.
	cloudConnName = "vizier-cloud-connector"
	// The label for PEMs.
	vizierPemLabel = "vizier-pem"
	// The name of the metadata-pvc.
	metadataPVC = "metadata-pv-claim"
	// The name of the nats pod.
	natsName = "pl-nats-1"
	// How often we should ping the vizier pods for status updates.
	statuszCheckInterval = 20 * time.Second
)

// HTTPClient is the interface for a simple HTTPClient which can execute "Get".
type HTTPClient interface {
	Get(string) (resp *http.Response, err error)
}

type podWithEvents struct {
	pod    *v1.Pod
	events []v1.Event
}

// concurrentPodMap wraps a map with concurrency safe read/write operations.
// Most operations can be done with the methods. However, if you need to manipulate
// an entire child map, manually hold the mutex instead of writing a new method.
type concurrentPodMap struct {
	// mapping from the k8s label to the map of matching pods to their pod info.
	unsafeMap map[string]map[string]*podWithEvents
	mapMu     sync.Mutex
}

func (c *concurrentPodMap) delete(nameLabel string, k8sName string) {
	c.mapMu.Lock()
	defer c.mapMu.Unlock()
	labelMap, ok := c.unsafeMap[nameLabel]
	if !ok {
		return
	}
	delete(labelMap, k8sName)
}

func (c *concurrentPodMap) write(nameLabel, k8sName string, p *podWithEvents) {
	c.mapMu.Lock()
	defer c.mapMu.Unlock()
	labelMap, ok := c.unsafeMap[nameLabel]
	if !ok {
		labelMap = make(map[string]*podWithEvents)
		c.unsafeMap[nameLabel] = labelMap
	}
	labelMap[k8sName] = p
}

// concurrentPVCMap wraps a map with concurrency safe read/write operations.
type concurrentPVCMap struct {
	unsafeMap map[string]*v1.PersistentVolumeClaim
	mapMu     sync.Mutex
}

func (c *concurrentPVCMap) read(name string) (*v1.PersistentVolumeClaim, bool) {
	c.mapMu.Lock()
	defer c.mapMu.Unlock()
	pod, ok := c.unsafeMap[name]
	return pod, ok
}

func (c *concurrentPVCMap) write(name string, pod *v1.PersistentVolumeClaim) {
	c.mapMu.Lock()
	defer c.mapMu.Unlock()
	c.unsafeMap[name] = pod
}

// VizierMonitor is responsible for watching the k8s API and statusz endpoints to compile a reason and state
// for the overall Vizier instance.
type VizierMonitor struct {
	clientset   kubernetes.Interface
	httpClient  HTTPClient
	ctx         context.Context
	cancel      func()
	cloudClient *grpc.ClientConn

	namespace      string
	namespacedName types.NamespacedName

	podStates *concurrentPodMap
	lastPodRV string

	pvcStates *concurrentPVCMap
	lastPVCRV string

	vzUpdate func(context.Context, client.Object, ...client.UpdateOption) error
	vzGet    func(context.Context, types.NamespacedName, client.Object) error
}

// InitAndStartMonitor initializes and starts the status monitor for the Vizier.
func (m *VizierMonitor) InitAndStartMonitor(cloudClient *grpc.ClientConn) error {
	// Initialize current state.
	tr := &http.Transport{
		TLSClientConfig: &tls.Config{InsecureSkipVerify: true},
	}
	m.httpClient = &http.Client{Transport: tr}
	m.cloudClient = cloudClient
	m.ctx, m.cancel = context.WithCancel(context.Background())
	m.podStates = &concurrentPodMap{unsafeMap: make(map[string]map[string]*podWithEvents)}
	m.pvcStates = &concurrentPVCMap{unsafeMap: make(map[string]*v1.PersistentVolumeClaim)}

	err := m.initState()
	if err != nil {
		return err
	}

	// Watch for future updates in the namespace.
	go m.watchK8sPods()

	// Watch for future PVC updates.
	go m.watchK8sPVC()

	// Start goroutine for periodically pinging statusz endpoints and
	// reconciling the Vizier status.
	go m.runReconciler()

	return nil
}

func (m *VizierMonitor) initState() error {
	podList, err := m.clientset.CoreV1().Pods(m.namespace).List(m.ctx, metav1.ListOptions{})
	if err != nil {
		return err
	}
	m.lastPodRV = podList.ResourceVersion
	// Populate vizierStates with current pod state.
	for _, pod := range podList.Items {
		m.handlePod(pod)
	}

	pvcList, err := m.clientset.CoreV1().PersistentVolumeClaims(m.namespace).List(m.ctx, metav1.ListOptions{})
	if err != nil {
		return err
	}
	m.lastPVCRV = pvcList.ResourceVersion

	for _, pvc := range pvcList.Items {
		m.handlePVC(pvc)
	}

	return nil
}

func (m *VizierMonitor) handlePod(p v1.Pod) {
	// We label all of our vizier pods with a name=<componentName>.
	// For now, this assumes no replicas. If a new pod starts up, it will replace
	// the status of the previous pod.
	// In the future we may add special handling for PEMs/multiple kelvins.
	nameLabel, ok := p.ObjectMeta.Labels["name"]
	if !ok {
		nameLabel = ""
	}
	k8sName := p.ObjectMeta.Name

	// Delete from our pemState if the pod is set for deletion.
	if p.ObjectMeta.DeletionTimestamp != nil {
		m.podStates.delete(nameLabel, k8sName)
		return
	}

	podObj := &podWithEvents{pod: &p}
	// Avoid getting events if pod is running. We don't need the extra debugging info.
	if p.Status.Phase != v1.PodRunning {
		eventsInterface := m.clientset.CoreV1().Events(m.namespace)
		selector := eventsInterface.GetFieldSelector(&p.Name, &m.namespace, nil, nil)
		eventList, err := eventsInterface.List(context.Background(), metav1.ListOptions{
			FieldSelector: selector.String(),
		})
		if err != nil {
			log.WithError(err).Error("unable to get event list")
		}
		podObj.events = eventList.Items
	}
	m.podStates.write(nameLabel, k8sName, podObj)
}

func (m *VizierMonitor) handlePVC(pvc v1.PersistentVolumeClaim) {
	m.pvcStates.write(pvc.ObjectMeta.Name, &pvc)
}

func (m *VizierMonitor) watchK8sPods() {
	for {
		watcher := cache.NewListWatchFromClient(m.clientset.CoreV1().RESTClient(), "pods", m.namespace, fields.Everything())
		retryWatcher, err := watch.NewRetryWatcher(m.lastPodRV, watcher)
		if err != nil {
			log.WithError(err).Fatal("Could not start watcher for pods")
		}

		resCh := retryWatcher.ResultChan()
		for {
			select {
			case <-m.ctx.Done():
				log.Info("Received cancel, stopping K8s watcher")
				return
			case c := <-resCh:
				s, ok := c.Object.(*metav1.Status)
				if ok && s.Status == metav1.StatusFailure {
					log.WithField("status", s.Status).Info("Received failure status in watcher")
					// Try to start up another watcher instance.
					break
				}

				// Update the lastPodRV, so that if the watcher restarts, it starts at the correct resource version.
				o, ok := c.Object.(*v1.Pod)
				if !ok {
					continue
				}

				m.lastPodRV = o.ObjectMeta.ResourceVersion

				m.handlePod(*o)
			}
		}
	}
}

func (m *VizierMonitor) watchK8sPVC() {
	for {
		watcher := cache.NewListWatchFromClient(m.clientset.CoreV1().RESTClient(), "persistentvolumeclaims", m.namespace, fields.Everything())
		retryWatcher, err := watch.NewRetryWatcher(m.lastPVCRV, watcher)
		if err != nil {
			log.WithError(err).Fatal("Could not start watcher for pvcs")
		}

		resCh := retryWatcher.ResultChan()
		for {
			select {
			case <-m.ctx.Done():
				log.Info("Received cancel, stopping K8s watcher")
				return
			case c := <-resCh:
				s, ok := c.Object.(*metav1.Status)
				if ok && s.Status == metav1.StatusFailure {
					log.WithField("status", s.Status).Info("Received failure status in watcher")
					// Try to start up another watcher instance.
					break
				}

				// Update the lastPVCRV, so that if the watcher restarts, it starts at the correct resource version.
				o, ok := c.Object.(*v1.PersistentVolumeClaim)
				if !ok {
					continue
				}

				m.lastPVCRV = o.ObjectMeta.ResourceVersion
				m.handlePVC(*o)
			}
		}
	}
}

// vizierState details the state of Vizier at a snapshot.
type vizierState struct {
	// Reason is the description of the state. Should only be set with values enumerated in `src/shared/status/vzstatus.go`
	Reason status.VizierReason
}

func okState() *vizierState {
	return &vizierState{Reason: ""}
}

func isOk(state *vizierState) bool {
	return state.Reason == okState().Reason
}

// getNATSState determines the state of nats then translates
// that to a corresponding VizierState.
func getNATSState(client HTTPClient, pods *concurrentPodMap) *vizierState {
	pods.mapMu.Lock()
	defer pods.mapMu.Unlock()

	noLabelPods, ok := pods.unsafeMap[""]
	if !ok {
		return &vizierState{Reason: status.NATSPodMissing}
	}

	natsPod, ok := noLabelPods[natsName]
	if !ok {
		return &vizierState{Reason: status.NATSPodMissing}
	}

	if natsPod.pod.Status.Phase == v1.PodPending {
		return &vizierState{Reason: status.NATSPodPending}
	}

	if natsPod.pod.Status.Phase != v1.PodRunning {
		return &vizierState{Reason: status.NATSPodFailed}
	}

	resp, err := client.Get(fmt.Sprintf("http://%s:%d/", natsPod.pod.Status.PodIP, 8222))
	if err != nil {
		log.WithError(err).Error("Error making nats monitoring call")
		return &vizierState{Reason: status.NATSPodFailed}
	}
	defer resp.Body.Close()

	if resp.StatusCode != http.StatusOK {
		return &vizierState{Reason: status.NATSPodFailed}
	}

	// Return the value of the cloud connector.
	return okState()
}

// getCloudConnState determines the state of the cloud connector then translates
// that to a corresponding VizierState.
func getCloudConnState(client HTTPClient, pods *concurrentPodMap) *vizierState {
	pods.mapMu.Lock()
	defer pods.mapMu.Unlock()
	labelMap, ok := pods.unsafeMap[cloudConnName]
	if !ok || len(labelMap) == 0 {
		return &vizierState{Reason: status.CloudConnectorMissing}
	}
	// We iterate here with the assumption that if any cc pods are pending the whole thing should fail.
	// This should account for failed updates, or catching the cluster during an upgrade.
	for _, ccPod := range labelMap {
		if ccPod.pod.Status.Phase == v1.PodPending {
			return &vizierState{Reason: status.CloudConnectorPodPending}
		}

		if ccPod.pod.Status.Phase != v1.PodRunning {
			return &vizierState{Reason: status.CloudConnectorPodFailed}
		}
		// Ping cloudConn's statusz.
		ok, podStatus := queryPodStatusz(client, ccPod.pod)
		if !ok {
			return &vizierState{Reason: status.VizierReason(podStatus)}
		}
	}

	return okState()
}

// getControlPlanePodState determines the state of control plane pods,
// returning a pending state if the pods are stuck
func getControlPlanePodState(pods *concurrentPodMap) *vizierState {
	pods.mapMu.Lock()
	defer pods.mapMu.Unlock()
	for nameLabel, labelMap := range pods.unsafeMap {
		// Skip reading about viziers because they are the most data intensive part.
		if nameLabel == vizierPemLabel {
			continue
		}
		for _, p := range labelMap {
			plane, ok := p.pod.ObjectMeta.Labels["plane"]
			// We only want to check pods that are not data plane pods.
			if ok && plane == "data" {
				continue
			}
			if p.pod.Status.Phase == v1.PodPending {
				return &vizierState{Reason: status.ControlPlanePodsPending}
			}
			if p.pod.Status.Phase != v1.PodRunning && p.pod.Status.Phase != v1.PodSucceeded {
				return &vizierState{Reason: status.ControlPlanePodsFailed}
			}
		}
	}

	return okState()
}

// Returns whether the storage class name requested by the pvc is valid for the Kubernetes instance.
func isValidPVC(clientset kubernetes.Interface, pvc *v1.PersistentVolumeClaim) (bool, error) {
	classes, err := k8s.ListStorageClasses(clientset)
	if err != nil {
		return false, err
	}
	for _, c := range classes.Items {
		if c.ObjectMeta.Name == *pvc.Spec.StorageClassName {
			return true, nil
		}
	}
	return false, nil
}

// getMetadataPVCState determines the state of the PVC then translates it to a corresponding vizierState.
func getMetadataPVCState(clientset kubernetes.Interface, pvcMap *concurrentPVCMap) *vizierState {
	pvc, ok := pvcMap.read(metadataPVC)
	if !ok {
		return &vizierState{Reason: status.MetadataPVCMissing}
	}
	if pvc.Status.Phase == v1.ClaimPending {
		isvalidClass, err := isValidPVC(clientset, pvc)
		if err != nil {
			log.WithError(err).Error("unable to list storage classes")
			return &vizierState{Reason: status.MetadataPVCStorageClassUnavailable}
		}
		if !isvalidClass {
			return &vizierState{Reason: status.MetadataPVCStorageClassUnavailable}
		}
		return &vizierState{Reason: status.MetadataPVCPendingBinding}
	}
	return okState()
}

// getPEMResourceLimitsState reads the state of pem resource limits to make sure they're running as expected.
func getPEMResourceLimitsState(pods *concurrentPodMap) *vizierState {
	pods.mapMu.Lock()
	defer pods.mapMu.Unlock()
	pems, ok := pods.unsafeMap[vizierPemLabel]
	if !ok || len(pems) == 0 {
		return &vizierState{Reason: status.PEMsMissing}
	}

	re := regexp.MustCompile("Insufficient memory")
	pemInsufficientMemory := 0
	for _, pem := range pems {
		if pem.pod.Status.Phase == v1.PodRunning {
			continue
		}
		// Check for an insufficient memory event for this pending pod.
		for _, event := range pem.events {
			if event.Reason == "FailedScheduling" && re.MatchString(event.Message) {
				pemInsufficientMemory++
				break
			}
		}
	}
	if pemInsufficientMemory == len(pems) {
		return &vizierState{Reason: status.PEMsAllInsufficientMemory}
	}
	if pemInsufficientMemory > 0 {
		return &vizierState{Reason: status.PEMsSomeInsufficientMemory}
	}

	return okState()
}

// getVizierVersionState gets the version of the running Vizier and compares it to the latest version of Vizier.
// If the vizier version is more than one major version too old, then the cluster is in a degraded state.
func getVizierVersionState(atClient cloudpb.ArtifactTrackerClient, vz *pixiev1alpha1.Vizier) *vizierState {
	latest, err := getLatestVizierVersion(context.Background(), atClient)
	if err != nil {
		log.WithError(err).Error("Failed to get latest vizier version")
		return nil
	}

	current := vz.Status.Version
	if current == "" {
		log.Error("No version specified on Vizier CRD status")
		return nil
	}

	currentSemVer, err := semver.Make(current)
	if err != nil {
		log.WithError(err).Error("Failed to parse current Vizier version")
		return nil
	}
	latestSemVer, err := semver.Make(latest)
	if err != nil {
		log.WithError(err).Error("Failed to parse latest Vizier version")
		return nil
	}

	devVersionRange, _ := semver.ParseRange("<=0.0.0")
	if devVersionRange(currentSemVer) {
		return okState() // We consider dev versions up-to-date.
	}

	if currentSemVer.Major != latestSemVer.Major || currentSemVer.Minor <= latestSemVer.Minor-2 {
		return &vizierState{Reason: status.VizierVersionTooOld}
	}
	return okState()
}

// getvizierState determines the state of the  Vizier instance based on the snapshot
// of data available at call time. Reports the first state that fails (does not aggregate),
// otherwise reports a healthy state.
func (m *VizierMonitor) getvizierState(vz *pixiev1alpha1.Vizier) *vizierState {
	// Check the latest vizier version, and current vizier version first. Regardless of
	// whether the vizier pods are running, we consider the cluster in a degraded state.
	atClient := cloudpb.NewArtifactTrackerClient(m.cloudClient)
	vzVersionState := getVizierVersionState(atClient, vz)
	if vzVersionState != nil && !isOk(vzVersionState) {
		return vzVersionState
	}

	if !vz.Spec.UseEtcdOperator {
		pvcState := getMetadataPVCState(m.clientset, m.pvcStates)
		if !isOk(pvcState) {
			return pvcState
		}
	}

	podState := getControlPlanePodState(m.podStates)
	if !isOk(podState) {
		return podState
	}

	natsState := getNATSState(m.httpClient, m.podStates)
	if !isOk(natsState) {
		return natsState
	}

	pemState := getPEMResourceLimitsState(m.podStates)
	if !isOk(pemState) {
		return pemState
	}

	ccState := getCloudConnState(m.httpClient, m.podStates)
	if !isOk(ccState) {
		return ccState
	}

	return okState()
}

// translateReasonToPhase maps a specific VizierReason into a more general VizierPhase.
// Empty reasons are considered healthy and unmatched reasons are by default unhealthy.
func translateReasonToPhase(reason status.VizierReason) pixiev1alpha1.VizierPhase {
	if reason == "" {
		return pixiev1alpha1.VizierPhaseHealthy
	}
	if reason == status.CloudConnectorPodPending {
		return pixiev1alpha1.VizierPhaseUpdating
	}
	if reason == status.MetadataPVCPendingBinding {
		return pixiev1alpha1.VizierPhaseUpdating
	}
	if reason == status.NATSPodPending {
		return pixiev1alpha1.VizierPhaseUpdating
	}
	if reason == status.VizierVersionTooOld {
		return pixiev1alpha1.VizierPhaseUnhealthy
	}
	if reason == status.CloudConnectorPodPending || reason == status.MetadataPVCPendingBinding || reason == status.ControlPlanePodsPending {
		return pixiev1alpha1.VizierPhaseUpdating
	}
	if reason == status.CloudConnectorMissing {
		return pixiev1alpha1.VizierPhaseDisconnected
	}
	if reason == status.PEMsSomeInsufficientMemory {
		return pixiev1alpha1.VizierPhaseDegraded
	}
	return pixiev1alpha1.VizierPhaseUnhealthy
}

// runReconciler periodically evaluates the state of the Vizier Cluster and sends the state as an update.
func (m *VizierMonitor) runReconciler() {
	t := time.NewTicker(statuszCheckInterval)
	for {
		select {
		case <-m.ctx.Done():
			log.Info("Received cancel, stopping status reconciler")
			return
		case <-t.C:
			vz := &pixiev1alpha1.Vizier{}
			err := m.vzGet(context.Background(), m.namespacedName, vz)
			if err != nil {
				log.WithError(err).Error("Failed to get vizier")
				continue
			}

			vizierState := m.getvizierState(vz)

			vz.Status.VizierPhase = translateReasonToPhase(vizierState.Reason)
			vz.Status.VizierReason = string(vizierState.Reason)
			vz.Status.Message = status.GetMessageFromReason(vizierState.Reason)
			err = m.vzUpdate(context.Background(), vz)
			if err != nil {
				log.WithError(err).Error("Failed to update vizier status")
			}
		}
	}
}

// queryPodStatusz returns a pod's self-reported status as served by its statusz endpoint.
func queryPodStatusz(client HTTPClient, pod *v1.Pod) (bool, string) {
	podIP := pod.Status.PodIP
	// Assume that the statusz endpoint is on the first port in the first container.
	var port int32
	if len(pod.Spec.Containers) > 0 && len(pod.Spec.Containers[0].Ports) > 0 {
		port = pod.Spec.Containers[0].Ports[0].ContainerPort
	}

	resp, err := client.Get(fmt.Sprintf("https://%s:%d/statusz", podIP, port))
	if err != nil {
		log.WithError(err).Error("Error making statusz call")
		return false, ""
	}
	defer resp.Body.Close()

	if resp.StatusCode == http.StatusOK {
		return true, ""
	}
	// This is for backwards compatibility for cloudconnectors who do not yet have a statusz endpoint.
	// We should assume a healthy state if the pod is running.
	if resp.StatusCode != http.StatusServiceUnavailable {
		return true, ""
	}

	body, err := io.ReadAll(resp.Body)

	if err != nil {
		log.WithError(err).Error("Error reading the response body")
		return false, ""
	}

	return false, strings.TrimSpace(string(body))
}

// Quit stops the VizierMonitor from monitoring the vizier in the given namespace.
func (m *VizierMonitor) Quit() {
	if m.ctx != nil {
		m.cancel()
	}
}
