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
	"crypto/x509"
	"encoding/pem"
	"fmt"
	"io"
	"net"
	"net/http"
	"net/url"
	"regexp"
	"strings"
	"sync"
	"time"

	"github.com/blang/semver"
	log "github.com/sirupsen/logrus"
	"google.golang.org/grpc"
	v1 "k8s.io/api/core/v1"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/apimachinery/pkg/types"
	"k8s.io/client-go/informers"
	"k8s.io/client-go/kubernetes"
	"k8s.io/client-go/rest"
	"k8s.io/client-go/tools/cache"
	"sigs.k8s.io/controller-runtime/pkg/client"

	"px.dev/pixie/src/api/proto/cloudpb"
	"px.dev/pixie/src/operator/apis/px.dev/v1alpha1"
	pixiev1alpha1 "px.dev/pixie/src/operator/apis/px.dev/v1alpha1"
	"px.dev/pixie/src/shared/status"
	"px.dev/pixie/src/utils/shared/k8s"
)

const (
	// The name label of the cloud-conn pod.
	cloudConnName = "vizier-cloud-connector"
	// The name label for PEMs.
	vizierPemLabel = "vizier-pem"
	// The name label for metadata pods.
	vizierMetadataLabel = "vizier-metadata"
	// The timeout for pending metadata pods.
	vizierMetadataTimeout = 5 * time.Minute
	// The name label for nats pods.
	natsLabel = "pl-nats"
	// The name of the nats pod.
	natsPodName = "pl-nats-0"
	// How often we should ping the vizier pods for status updates.
	statuszCheckInterval = 20 * time.Second
	// The threshold of number of crashing PEM pods before we declare a cluster degraded.
	pemCrashingThreshold = 0.25
	// The number of times etcd should crash before we try to autorepair.
	etcdCrashLimit = 5
)

// HTTPClient is the interface for a simple HTTPClient which can execute "Get".
type HTTPClient interface {
	Get(string) (resp *http.Response, err error)
}

type podWrapper struct {
	pod *v1.Pod
}

// concurrentPodMap wraps a map with concurrency safe read/write operations.
// Most operations can be done with the methods. However, if you need to manipulate
// an entire child map, manually hold the mutex instead of writing a new method.
type concurrentPodMap struct {
	// mapping from the k8s label to the map of matching pods to their pod info.
	unsafeMap map[string]map[string]*podWrapper
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

func (c *concurrentPodMap) write(nameLabel, k8sName string, p *podWrapper) {
	c.mapMu.Lock()
	defer c.mapMu.Unlock()
	labelMap, ok := c.unsafeMap[nameLabel]
	if !ok {
		labelMap = make(map[string]*podWrapper)
		c.unsafeMap[nameLabel] = labelMap
	}
	labelMap[k8sName] = p
}

// VizierMonitor is responsible for watching the k8s API and statusz endpoints to compile a reason and state
// for the overall Vizier instance.
type VizierMonitor struct {
	clientset   kubernetes.Interface
	restConfig  *rest.Config
	factory     informers.SharedInformerFactory
	httpClient  HTTPClient
	ctx         context.Context
	cancel      func()
	cloudClient *grpc.ClientConn

	namespace         string
	namespacedName    types.NamespacedName
	devCloudNamespace string

	podStates *concurrentPodMap
	nodeState *vizierState
	pvcState  *vizierState
	certState *vizierState

	vzUpdate     func(context.Context, client.Object, ...client.SubResourceUpdateOption) error
	vzGet        func(context.Context, types.NamespacedName, client.Object, ...client.GetOption) error
	vzSpecUpdate func(context.Context, client.Object, ...client.UpdateOption) error
}

// InitAndStartMonitor initializes and starts the status monitor for the Vizier.
func (m *VizierMonitor) InitAndStartMonitor(cloudClient *grpc.ClientConn) {
	// Initialize current state.
	tr := http.DefaultTransport.(*http.Transport).Clone()
	tr.TLSClientConfig = m.getTLSConfig()
	m.httpClient = &http.Client{Transport: tr}
	m.cloudClient = cloudClient
	m.ctx, m.cancel = context.WithCancel(context.Background())
	m.podStates = &concurrentPodMap{unsafeMap: make(map[string]map[string]*podWrapper)}

	m.nodeState = okState()
	m.pvcState = okState()
	m.certState = okState()

	m.factory = informers.NewSharedInformerFactoryWithOptions(m.clientset, 0, informers.WithNamespace(m.namespace))

	// Watch for pod updates in the namespace.
	go m.watchK8sPods()

	m.watchCerts()

	// Start PVC monitor.
	pvcStateCh := make(chan *vizierState)
	pvcW := &pvcWatcher{
		clientset: m.clientset,
		factory:   m.factory,
		namespace: m.namespace,
		state:     pvcStateCh,
	}
	go pvcW.start(m.ctx)

	// Start node monitor.
	nodeStateCh := make(chan *vizierState)
	nodeW := &nodeWatcher{
		factory: m.factory,
		state:   nodeStateCh,
	}
	go nodeW.start(m.ctx)

	// Start goroutine for periodically pinging statusz endpoints and
	// reconciling the Vizier status.
	go m.statusAggregator(nodeStateCh, pvcStateCh)
	go m.runReconciler()
}

func (m *VizierMonitor) onAddPod(obj interface{}) {
	pod, ok := obj.(*v1.Pod)
	if !ok {
		return
	}
	if pod.Status.Phase == v1.PodFailed {
		// Don't include failed pods, we only care about the currently running or pending pod.
		return
	}
	m.podStates.write(pod.ObjectMeta.Labels["name"], pod.ObjectMeta.Name, &podWrapper{pod: pod})
}

func (m *VizierMonitor) onUpdatePod(oldObj, newObj interface{}) {
	pod, ok := newObj.(*v1.Pod)
	if !ok {
		return
	}
	if pod.Status.Phase == v1.PodFailed {
		// Remove failed pods, we only care about the currently running or pending pod.
		m.podStates.delete(pod.ObjectMeta.Labels["name"], pod.ObjectMeta.Name)
		return
	}
	m.podStates.write(pod.ObjectMeta.Labels["name"], pod.ObjectMeta.Name, &podWrapper{pod: pod})
}

func (m *VizierMonitor) onDeletePod(obj interface{}) {
	pod, ok := obj.(*v1.Pod)
	if !ok {
		return
	}
	m.podStates.delete(pod.ObjectMeta.Labels["name"], pod.ObjectMeta.Name)
}

func (m *VizierMonitor) watchK8sPods() {
	informer := m.factory.Core().V1().Pods().Informer()
	stopper := make(chan struct{})
	defer close(stopper)
	_, _ = informer.AddEventHandler(cache.ResourceEventHandlerFuncs{
		AddFunc:    m.onAddPod,
		UpdateFunc: m.onUpdatePod,
		DeleteFunc: m.onDeletePod,
	})
	informer.Run(stopper)
}

func (m *VizierMonitor) watchCerts() {
	err := m.checkCerts()
	if err != nil {
		log.WithError(err).Error("Failed to check certs")
	}

	timer := time.NewTicker(24 * time.Hour)
	go func() {
		for {
			select {
			case <-m.ctx.Done():
				log.Info("Received cancel, stopping cert checker")
				return
			case <-timer.C:
				err := m.checkCerts()
				if err != nil {
					log.WithError(err).Error("Failed to check certs")
				}
			}
		}
	}()
}

func (m *VizierMonitor) checkCerts() error {
	tlsSecret, err := m.clientset.CoreV1().Secrets(m.namespace).Get(context.Background(), "service-tls-certs", metav1.GetOptions{})
	if err != nil {
		return err
	}
	cert, _ := pem.Decode(tlsSecret.Data["server.crt"])
	x509cert, err := x509.ParseCertificate(cert.Bytes)
	if err != nil {
		log.WithError(err).Error("failed to parse cert")
		return err
	}
	if time.Now().Add(5 * 24 * time.Hour).After(x509cert.NotAfter) {
		m.certState = &vizierState{Reason: status.TLSCertsExpired}
		return nil
	}
	m.certState = okState()
	return nil
}

func (m *VizierMonitor) getTLSConfig() *tls.Config {
	// This is used as a fallback incase we somehow fail to get the CA for the vizier.
	fallbackInsecureConfig := &tls.Config{InsecureSkipVerify: true} // lgtm [go/disabled-certificate-check]

	tlsSecret, err := m.clientset.CoreV1().Secrets(m.namespace).Get(context.Background(), "service-tls-certs", metav1.GetOptions{})
	if err != nil {
		log.WithError(err).Warn("failed to get certs secret, monitor will use insecure tls to check /statusz")
		return fallbackInsecureConfig
	}

	certPool := x509.NewCertPool()
	if ok := certPool.AppendCertsFromPEM(tlsSecret.Data["ca.crt"]); !ok {
		log.WithError(err).Warn("failed add CA to pool, monitor will use insecure tls to check /statusz")
		return fallbackInsecureConfig
	}

	return &tls.Config{RootCAs: certPool}
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

	natsPods, ok := pods.unsafeMap[natsLabel]
	if !ok {
		return &vizierState{Reason: status.NATSPodMissing}
	}

	natsPod, ok := natsPods[natsPodName]
	if !ok {
		return &vizierState{Reason: status.NATSPodMissing}
	}

	if natsPod.pod.Status.Phase == v1.PodPending {
		return &vizierState{Reason: status.NATSPodPending}
	}

	if natsPod.pod.Status.Phase != v1.PodRunning {
		return &vizierState{Reason: status.NATSPodFailed}
	}

	u := url.URL{
		Scheme: "http",
		Host:   net.JoinHostPort(k8s.GetPodAddr(*natsPod.pod), "8222"),
	}

	resp, err := client.Get(u.String())
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

// getEtcdState determines the state of etcd then translates that
// to a corresponding VizierState.
func getEtcdState(pods *concurrentPodMap) *vizierState {
	pods.mapMu.Lock()
	defer pods.mapMu.Unlock()

	unlabeledPods, ok := pods.unsafeMap[""]
	if !ok {
		return &vizierState{Reason: status.EtcdPodsMissing}
	}

	for podName, pod := range unlabeledPods {
		if !strings.HasPrefix(podName, "pl-etcd-") {
			continue
		}
		for _, c := range pod.pod.Status.ContainerStatuses {
			if c.State.Waiting != nil && c.State.Waiting.Reason == "CrashLoopBackOff" && c.RestartCount >= etcdCrashLimit {
				return &vizierState{Reason: status.EtcdPodsCrashing}
			}
		}
	}

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

// getStatefulMetadataPendingState returns whether the stateful metadata pod is pending.
func getStatefulMetadataPendingState(pods *concurrentPodMap, vz *v1alpha1.Vizier) *vizierState {
	// We wait for a timeout because pvc provisioning can take some time.
	if vz.Status.LastReconciliationPhaseTime == nil || time.Since(vz.Status.LastReconciliationPhaseTime.Time) < vizierMetadataTimeout {
		return okState()
	}
	pods.mapMu.Lock()
	defer pods.mapMu.Unlock()
	labelMap, ok := pods.unsafeMap[vizierMetadataLabel]
	// This should be covered in another state.
	if !ok || len(labelMap) == 0 {
		return okState()
	}
	for _, metadataPod := range labelMap {
		for _, ownerRef := range metadataPod.pod.OwnerReferences {
			if !(ownerRef.Kind == "StatefulSet") {
				continue
			}
			if metadataPod.pod.Status.Phase != v1.PodPending {
				return okState()
			}
			// The following checks whether the pod is waiting for the initcontainers to finish:
			// Check whether all of the initContainers have completed
			allInitContainersCompleted := true
			for _, initContainerStatus := range metadataPod.pod.Status.InitContainerStatuses {
				if initContainerStatus.State.Terminated == nil || initContainerStatus.State.Terminated.ExitCode != 0 {
					allInitContainersCompleted = false
					break
				}
			}
			if allInitContainersCompleted {
				return &vizierState{Reason: status.MetadataStatefulSetPodPending}
			}
		}
	}

	return okState()
}

// getControlPlanePodState determines the state of control plane pods,
// returning a pending state if the pods are stuck
func getControlPlanePodState(pods *concurrentPodMap) *vizierState {
	pods.mapMu.Lock()
	defer pods.mapMu.Unlock()
	taintRe := regexp.MustCompile("had taint.* that the pod didn't tolerate")
	for nameLabel, labelMap := range pods.unsafeMap {
		// Skip reading about viziers because they are the most data intensive part.
		if nameLabel == vizierPemLabel {
			continue
		}
		for _, p := range labelMap {
			// We only want to check control plane pods.
			if p.pod.ObjectMeta.Labels["plane"] != "control" {
				continue
			}
			if p.pod.Status.Phase == v1.PodPending {
				for _, cond := range p.pod.Status.Conditions {
					if cond.Type == v1.PodScheduled && cond.Status == v1.ConditionFalse && cond.Reason == v1.PodReasonUnschedulable {
						if taintRe.MatchString(cond.Message) {
							return &vizierState{Reason: status.ControlPlaneFailedToScheduleBecauseOfTaints}
						}
						return &vizierState{Reason: status.ControlPlaneFailedToSchedule}
					}
				}
				return &vizierState{Reason: status.ControlPlanePodsPending}
			}
			if p.pod.Status.Phase != v1.PodRunning && p.pod.Status.Phase != v1.PodSucceeded {
				return &vizierState{Reason: status.ControlPlanePodsFailed}
			}
		}
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

	memoryRe := regexp.MustCompile("Insufficient memory")
	pemInsufficientMemory := 0
	for _, pem := range pems {
		if pem.pod.Status.Phase == v1.PodRunning {
			continue
		}
		for _, cond := range pem.pod.Status.Conditions {
			if cond.Type == v1.PodScheduled && cond.Status == v1.ConditionFalse && cond.Reason == v1.PodReasonUnschedulable && memoryRe.MatchString(cond.Message) {
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

// getPEMCrashingState reads the state of running PEMs to see if a large portion are failing.
func getPEMCrashingState(pods *concurrentPodMap) *vizierState {
	pods.mapMu.Lock()
	defer pods.mapMu.Unlock()
	pems, ok := pods.unsafeMap[vizierPemLabel]
	if !ok || len(pems) == 0 {
		return &vizierState{Reason: status.PEMsMissing}
	}

	pemCrashing := 0.0
	for _, pem := range pems {
		if pem.pod.Status.Phase != v1.PodRunning {
			continue
		}
		for _, c := range pem.pod.Status.ContainerStatuses {
			if c.State.Terminated != nil && c.State.Terminated.Reason == "Error" {
				pemCrashing++
				break
			}
			if c.State.Waiting != nil && c.State.Waiting.Reason == "CrashLoopBackOff" {
				pemCrashing++
				break
			}
		}
	}
	numPems := float64(len(pems))
	if pemCrashing == numPems {
		return &vizierState{Reason: status.PEMsAllFailing}
	}
	if pemCrashing > numPems*pemCrashingThreshold {
		return &vizierState{Reason: status.PEMsHighFailureRate}
	}
	return okState()
}

// getVizierState determines the state of the Vizier instance based on the snapshot
// of data available at call time. Reports the first state that fails (does not aggregate),
// otherwise reports a healthy state.
func (m *VizierMonitor) getVizierState(vz *pixiev1alpha1.Vizier) *vizierState {
	// Check the latest vizier version, and current vizier version first. Regardless of
	// whether the vizier pods are running, we consider the cluster in a degraded state.
	atClient := cloudpb.NewArtifactTrackerClient(m.cloudClient)
	vzVersionState := getVizierVersionState(atClient, vz)
	if vzVersionState != nil && !isOk(vzVersionState) {
		return vzVersionState
	}

	if !isOk(m.certState) {
		return m.certState
	}

	if !vz.Spec.UseEtcdOperator && !isOk(m.pvcState) {
		return m.pvcState
	}

	// Only show the metadata state if etcd is not being used.
	ssMetadataState := getStatefulMetadataPendingState(m.podStates, vz)
	if !vz.Spec.UseEtcdOperator && !isOk(ssMetadataState) {
		return ssMetadataState
	}

	if !isOk(m.nodeState) {
		return m.nodeState
	}

	podState := getControlPlanePodState(m.podStates)
	if !isOk(podState) {
		return podState
	}

	natsState := getNATSState(m.httpClient, m.podStates)
	if !isOk(natsState) {
		return natsState
	}

	if vz.Spec.UseEtcdOperator {
		etcdState := getEtcdState(m.podStates)
		if !isOk(etcdState) {
			return etcdState
		}
	}

	pemResourceState := getPEMResourceLimitsState(m.podStates)
	if !isOk(pemResourceState) {
		return pemResourceState
	}

	pemCrashingState := getPEMCrashingState(m.podStates)
	if !isOk(pemCrashingState) {
		return pemCrashingState
	}

	ccState := getCloudConnState(m.httpClient, m.podStates)
	if !isOk(ccState) {
		return ccState
	}

	return okState()
}

func (m *VizierMonitor) statusAggregator(nodeStateCh, pvcStateCh <-chan *vizierState) {
	for {
		select {
		case <-m.ctx.Done():
			return
		case u := <-nodeStateCh:
			m.nodeState = u
		case u := <-pvcStateCh:
			m.pvcState = u
		}

		vz := &pixiev1alpha1.Vizier{}
		err := m.vzGet(context.Background(), m.namespacedName, vz)
		if err != nil {
			log.WithError(err).Error("Failed to get vizier")
			continue
		}
	}
}

func (m *VizierMonitor) repairVizier(state *vizierState) error {
	// Input validation: Return if state is good
	if state.Reason == "" {
		log.Warn("Vizier seems to have repaired itself")
		return nil
	}

	// Delete pod if nats pod failed
	if state.Reason == status.NATSPodFailed {
		err := m.clientset.CoreV1().Pods(m.namespace).Delete(m.ctx, natsPodName, metav1.DeleteOptions{})
		if err != nil {
			log.WithError(err).Error("Failed to delete NATS pod")
			return err
		}

		log.Info("NATS pod was successfully deleted")
	} else if state.Reason == status.MetadataPVCStorageClassUnavailable {
		log.WithField("reason", state.Reason).Info("Switching to etcd backed metadata store")

		vz := &pixiev1alpha1.Vizier{}
		err := m.vzGet(context.Background(), m.namespacedName, vz)
		if err != nil {
			log.WithError(err).Error("Failed to get vizier")
			return err
		}

		vz.Spec.UseEtcdOperator = true
		err = m.vzSpecUpdate(m.ctx, vz)
		if err != nil {
			log.WithError(err).Error("Failed to update spec with etcd operator usage")
			return err
		}

		log.Info("Successfully switched to etcd backed metadata store")
	} else if state.Reason == status.EtcdPodsCrashing {
		log.Info("Etcd detected to be crashing, attempting to restart etcd")
		// Delete etcd, deploy will trigger a new statefulset to startup.
		err := m.clientset.AppsV1().StatefulSets(m.namespace).Delete(m.ctx, "pl-etcd", metav1.DeleteOptions{})
		if err != nil {
			log.WithError(err).Error("Failed to delete etcd statefulset")
			return err
		}
		// Trigger redeploy.
		vz := &pixiev1alpha1.Vizier{}
		err = m.vzGet(context.Background(), m.namespacedName, vz)
		if err != nil {
			log.WithError(err).Error("Failed to get vizier")
			return err
		}
		if len(vz.Status.Checksum) > 2 {
			vz.Status.Checksum = vz.Status.Checksum[2:]
		}
		err = m.vzUpdate(context.Background(), vz)
		if err != nil {
			log.WithError(err).Error("Failed to update status with empty checksum")
			return err
		}
	} else if state.Reason == status.TLSCertsExpired {
		vz := &pixiev1alpha1.Vizier{}
		err := m.vzGet(context.Background(), m.namespacedName, vz)
		if err != nil {
			log.WithError(err).Error("Failed to fetch Vizier")
			return err
		}

		err = deployCerts(context.Background(), m.namespace, vz, m.clientset, m.restConfig, true)
		if err != nil {
			log.WithError(err).Error("Failed to update certs")
		}
		m.certState = okState()

		log.Info("Bouncing Vizier pods to get certs update")
		err = k8s.DeletePods(m.clientset, m.namespace, "")
		if err != nil {
			return err
		}
	}
	return nil
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

			vizierState := m.getVizierState(vz)
			vz.SetStatus(vizierState.Reason)

			err = m.vzUpdate(context.Background(), vz)
			if err != nil {
				log.WithError(err).Error("Failed to update vizier status")
			}

			if !isOk(vizierState) {
				err := m.repairVizier(vizierState)
				if err != nil {
					log.WithError(err).Info("Failed to autorepair vizier")
				}
			}
		}
	}
}

// queryPodStatusz returns a pod's self-reported status as served by its statusz endpoint.
func queryPodStatusz(client HTTPClient, pod *v1.Pod) (bool, string) {
	// Assume that the statusz endpoint is on the first port in the first container.
	var port int32
	if len(pod.Spec.Containers) > 0 && len(pod.Spec.Containers[0].Ports) > 0 {
		port = pod.Spec.Containers[0].Ports[0].ContainerPort
	}

	u := url.URL{
		Scheme: "https",
		Host:   net.JoinHostPort(k8s.GetPodAddr(*pod), fmt.Sprintf("%d", port)),
		Path:   "statusz",
	}
	resp, err := client.Get(u.String())
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
