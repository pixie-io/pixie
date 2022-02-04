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
	"crypto/rand"
	"errors"
	"fmt"
	"strings"
	"sync"
	"time"

	"github.com/cenkalti/backoff/v3"
	log "github.com/sirupsen/logrus"
	"google.golang.org/grpc"
	v1 "k8s.io/api/core/v1"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/apimachinery/pkg/apis/meta/v1/unstructured"
	"k8s.io/apimachinery/pkg/runtime"
	"k8s.io/apimachinery/pkg/watch"
	"k8s.io/client-go/kubernetes"
	"k8s.io/client-go/rest"
	ctrl "sigs.k8s.io/controller-runtime"
	"sigs.k8s.io/controller-runtime/pkg/client"

	"px.dev/pixie/src/api/proto/cloudpb"
	"px.dev/pixie/src/api/proto/vizierconfigpb"
	"px.dev/pixie/src/operator/apis/px.dev/v1alpha1"
	"px.dev/pixie/src/operator/vendored/etcd"
	"px.dev/pixie/src/operator/vendored/nats"
	"px.dev/pixie/src/shared/services"
	"px.dev/pixie/src/utils/shared/certs"
	"px.dev/pixie/src/utils/shared/k8s"
)

const (
	// This is the key for the annotation that the operator applies on all of its deployed resources for a CRD.
	operatorAnnotation  = "vizier-name"
	clusterSecretJWTKey = "jwt-signing-key"
	// updatingFailedTimeout is the amount of time we wait since an Updated started
	// before we consider the Update Failed.
	updatingFailedTimeout = 30 * time.Minute
	// How often we should check whether a Vizier update failed.
	updatingVizierCheckPeriod = 1 * time.Minute
)

// defaultClassAnnotationKey is the key in the annotation map which indicates
// a storage class is default.
var defaultClassAnnotationKeys = []string{"storageclass.kubernetes.io/is-default-class", "storageclass.beta.kubernetes.io/is-default-class"}

// VizierReconciler reconciles a Vizier object
type VizierReconciler struct {
	client.Client
	Scheme *runtime.Scheme

	Clientset  *kubernetes.Clientset
	RestConfig *rest.Config

	monitor *VizierMonitor
}

// +kubebuilder:rbac:groups=pixie.px.dev,resources=viziers,verbs=get;list;watch;create;update;patch;delete
// +kubebuilder:rbac:groups=pixie.px.dev,resources=viziers/status,verbs=get;update;patch

func getCloudClientConnection(cloudAddr string, devCloudNS string) (*grpc.ClientConn, error) {
	isInternal := false

	if devCloudNS != "" {
		cloudAddr = fmt.Sprintf("api-service.%s.svc.cluster.local:51200", devCloudNS)
		isInternal = true
	}

	dialOpts, err := services.GetGRPCClientDialOptsServerSideTLS(isInternal)
	if err != nil {
		return nil, err
	}

	c, err := grpc.Dial(cloudAddr, dialOpts...)
	if err != nil {
		return nil, err
	}

	return c, nil
}

func getLatestVizierVersion(ctx context.Context, client cloudpb.ArtifactTrackerClient) (string, error) {
	req := &cloudpb.GetArtifactListRequest{
		ArtifactName: "vizier",
		ArtifactType: cloudpb.AT_CONTAINER_SET_YAMLS,
		Limit:        1,
	}
	resp, err := client.GetArtifactList(ctx, req)
	if err != nil {
		return "", err
	}

	if len(resp.Artifact) != 1 {
		return "", errors.New("Could not find Vizier artifact")
	}

	return resp.Artifact[0].VersionStr, nil
}

// validateNumDefaultStorageClasses returns a boolean whether there is exactly
// 1 default storage class or not.
func validateNumDefaultStorageClasses(clientset *kubernetes.Clientset) (bool, error) {
	storageClasses, err := clientset.StorageV1().StorageClasses().List(context.Background(), metav1.ListOptions{})
	if err != nil {
		return false, err
	}

	defaultClassCount := 0

	// Check annotations map on each storage class to see if default is set to "true".
	for _, storageClass := range storageClasses.Items {
		annotationsMap := storageClass.GetAnnotations()
		for _, key := range defaultClassAnnotationKeys {
			if annotationsMap[key] == "true" {
				// It is possible for some storageClasses to have both the beta/non-beta annotation.
				// We break here so that we don't double count this storageClass.
				defaultClassCount++
				break
			}
		}
	}
	return defaultClassCount == 1, nil
}

// Reconcile updates the Vizier running in the cluster to match the expected state.
func (r *VizierReconciler) Reconcile(ctx context.Context, req ctrl.Request) (ctrl.Result, error) {
	log.WithField("req", req).Info("Reconciling...")

	// Fetch vizier CRD to determine what operation should be performed.
	var vizier v1alpha1.Vizier
	if err := r.Get(ctx, req.NamespacedName, &vizier); err != nil {
		err = r.deleteVizier(ctx, req)
		if err != nil {
			log.WithError(err).Info("Failed to delete Vizier instance")
		}

		if r.monitor != nil && r.monitor.namespace == req.Namespace {
			r.monitor.Quit()
			r.monitor = nil
		}
		// Vizier CRD deleted. The vizier instance should also be deleted.
		return ctrl.Result{}, err
	}

	if vizier.Status.VizierPhase == v1alpha1.VizierPhaseNone && vizier.Status.ReconciliationPhase == v1alpha1.ReconciliationPhaseNone {
		// We are creating a new vizier instance.
		err := r.createVizier(ctx, req, &vizier)
		if err != nil {
			log.WithError(err).Info("Failed to deploy new Vizier instance")
		}
		return ctrl.Result{}, err
	}

	err := r.updateVizier(ctx, req, &vizier)
	if err != nil {
		log.WithError(err).Info("Failed to update Vizier instance")
	}

	// Check if we are already monitoring this Vizier.
	if r.monitor == nil || r.monitor.namespace != req.Namespace {
		if r.monitor != nil {
			r.monitor.Quit()
			r.monitor = nil
		}

		r.monitor = &VizierMonitor{
			namespace:      req.Namespace,
			namespacedName: req.NamespacedName,
			vzUpdate:       r.Status().Update,
			vzGet:          r.Get,
			clientset:      r.Clientset,
		}
		cloudClient, err := getCloudClientConnection(vizier.Spec.CloudAddr, vizier.Spec.DevCloudNamespace)
		if err != nil {
			log.WithError(err).Fatal("Failed to initialize vizier monitor")
		}
		err = r.monitor.InitAndStartMonitor(cloudClient)
		if err != nil {
			log.WithError(err).Fatal("Failed to initialize vizier monitor")
		}
	}

	// Vizier CRD has been updated, and we should update the running vizier accordingly.
	return ctrl.Result{}, err
}

// updateVizier updates the vizier instance according to the spec. As of the current moment, we only support updates to the Vizier version.
// Other updates to the Vizier spec will be ignored.
func (r *VizierReconciler) updateVizier(ctx context.Context, req ctrl.Request, vz *v1alpha1.Vizier) error {
	// TODO: We currently only trigger updates on changing Vizier versions. We should add a webhook
	// to disallow changes to other fields.
	if vz.Status.Version == vz.Spec.Version {
		log.Info("Versions matched, nothing to do")
		return nil
	}

	if vz.Status.ReconciliationPhase == v1alpha1.ReconciliationPhaseUpdating {
		log.Info("Already in the process of updating, nothing to do")
		return nil
	}

	return r.deployVizier(ctx, req, vz, true)
}

// deleteVizier deletes the vizier instance in the given namespace.
func (r *VizierReconciler) deleteVizier(ctx context.Context, req ctrl.Request) error {
	log.WithField("req", req).Info("Deleting Vizier...")
	od := k8s.ObjectDeleter{
		Namespace:  req.Namespace,
		Clientset:  r.Clientset,
		RestConfig: r.RestConfig,
		Timeout:    2 * time.Minute,
	}

	keyValueLabel := operatorAnnotation + "=" + req.Name
	_, _ = od.DeleteByLabel(keyValueLabel)
	// TODO(vihang): Remove the rest of these since we no longer use operator
	// managed NATS/etcd. Remove no sooner than 2021-10-15.
	_, _ = od.DeleteByLabel("app=nats")
	_, _ = od.DeleteByLabel("etcd_cluster=pl-etcd")
	_ = od.DeleteCustomObject("NatsCluster", "pl-nats")
	return nil
}

// createVizier deploys a new vizier instance in the given namespace.
func (r *VizierReconciler) createVizier(ctx context.Context, req ctrl.Request, vz *v1alpha1.Vizier) error {
	log.Info("Creating a new vizier instance")
	cloudClient, err := getCloudClientConnection(vz.Spec.CloudAddr, vz.Spec.DevCloudNamespace)
	if err != nil {
		return err
	}

	// If no version is set, we should fetch the latest version. This will trigger another reconcile that will do
	// the actual vizier deployment.
	if vz.Spec.Version == "" {
		atClient := cloudpb.NewArtifactTrackerClient(cloudClient)
		latest, err := getLatestVizierVersion(ctx, atClient)
		if err != nil {
			return err
		}
		vz.Spec.Version = latest
		err = r.Update(ctx, vz)
		if err != nil {
			return err
		}
		return nil
	}

	return r.deployVizier(ctx, req, vz, false)
}

// setReconciliationPhase sets the requested phase in the status and also sets the time to Now.
func setReconciliationPhase(vz *v1alpha1.Vizier, rp v1alpha1.ReconciliationPhase) *v1alpha1.Vizier {
	vz.Status.ReconciliationPhase = rp
	timeNow := metav1.Now()
	vz.Status.LastReconciliationPhaseTime = &timeNow
	return vz
}

func (r *VizierReconciler) deployVizier(ctx context.Context, req ctrl.Request, vz *v1alpha1.Vizier, update bool) error {
	log.Info("Starting a vizier deploy")
	cloudClient, err := getCloudClientConnection(vz.Spec.CloudAddr, vz.Spec.DevCloudNamespace)
	if err != nil {
		return err
	}

	// Set the status of the Vizier.
	vz = setReconciliationPhase(vz, v1alpha1.ReconciliationPhaseUpdating)
	err = r.Status().Update(ctx, vz)
	if err != nil {
		return err
	}

	// Add an additional annotation to our deployed vizier-resources, to allow easier tracking of the vizier resources.
	if vz.Spec.Pod == nil {
		vz.Spec.Pod = &v1alpha1.PodPolicy{}
	}

	if vz.Spec.Pod.Annotations == nil {
		vz.Spec.Pod.Annotations = make(map[string]string)
	}

	if vz.Spec.Pod.Labels == nil {
		vz.Spec.Pod.Labels = make(map[string]string)
	}

	if vz.Spec.Pod.NodeSelector == nil {
		vz.Spec.Pod.NodeSelector = make(map[string]string)
	}

	if !vz.Spec.UseEtcdOperator {
		// Check if the cluster offers PVC support.
		// If it does not, we should default to using the etcd operator, which does not
		// require PVC support.
		defaultStorageExists, err := validateNumDefaultStorageClasses(r.Clientset)
		if err != nil {
			log.WithError(err).Error("Error checking default storage classes")
		}
		if !defaultStorageExists {
			log.Warn("No default storage class detected for cluster. Deploying etcd operator instead of statefulset for metadata backend.")
			vz.Spec.UseEtcdOperator = true
		}
	}

	vz.Spec.Pod.Annotations[operatorAnnotation] = req.Name
	vz.Spec.Pod.Labels[operatorAnnotation] = req.Name

	// Update the spec in the k8s api as other parts of the code expect this to be true.
	err = r.Update(ctx, vz)
	if err != nil {
		return err
	}

	configForVizierResp, err := generateVizierYAMLsConfig(ctx, req.Namespace, vz, cloudClient)
	if err != nil {
		return err
	}
	yamlMap := configForVizierResp.NameToYamlContent

	// Update Vizier CRD status sentryDSN so that it can be accessed by other
	// vizier pods.
	vz.Status.SentryDSN = configForVizierResp.SentryDSN

	if !update {
		err = r.deployVizierConfigs(ctx, req.Namespace, vz, yamlMap)
		if err != nil {
			return err
		}

		err = r.deployVizierCerts(ctx, req.Namespace, vz)
		if err != nil {
			return err
		}

		err = r.deployVizierDeps(ctx, req.Namespace, vz, yamlMap)
		if err != nil {
			return err
		}
	} else {
		err = r.replaceOperatorManagedNATS(ctx, req.Namespace, vz, yamlMap)
		if err != nil {
			return err
		}
		err = r.replaceOperatorManagedEtcd(ctx, req.Namespace, vz, yamlMap)
		if err != nil {
			return err
		}
	}

	err = r.deployVizierCore(ctx, req.Namespace, vz, yamlMap, update)
	if err != nil {
		return err
	}

	// TODO(michellenguyen): Remove when the operator has the ability to ping CloudConn for Vizier Version.
	// We are currently blindly assuming that the new version is correct.
	_ = waitForCluster(r.Clientset, req.Namespace)

	// Refetch the Vizier resource, as it may have changed in the time in which we were waiting for the cluster.
	err = r.Get(ctx, req.NamespacedName, vz)
	if err != nil {
		// The Vizier was deleted in the meantime. Do nothing.
		return nil
	}

	vz.Status.Version = vz.Spec.Version
	vz = setReconciliationPhase(vz, v1alpha1.ReconciliationPhaseReady)

	err = r.Status().Update(ctx, vz)
	if err != nil {
		return err
	}

	return nil
}

// TODO(michellenguyen): Add a goroutine
// which checks when certs are about to expire. If they are about to expire,
// we should generate new certs and bounce all pods.
func (r *VizierReconciler) deployVizierCerts(ctx context.Context, namespace string, vz *v1alpha1.Vizier) error {
	log.Info("Generating certs")

	// Assign JWT signing key.
	jwtSigningKey := make([]byte, 64)
	_, err := rand.Read(jwtSigningKey)
	if err != nil {
		return err
	}
	s := k8s.GetSecret(r.Clientset, namespace, "pl-cluster-secrets")
	if s == nil {
		return errors.New("pl-cluster-secrets does not exist")
	}
	s.Data[clusterSecretJWTKey] = []byte(fmt.Sprintf("%x", jwtSigningKey))

	_, err = r.Clientset.CoreV1().Secrets(namespace).Update(ctx, s, metav1.UpdateOptions{})
	if err != nil {
		return err
	}

	certYAMLs, err := certs.GenerateVizierCertYAMLs(namespace)
	if err != nil {
		return err
	}

	resources, err := k8s.GetResourcesFromYAML(strings.NewReader(certYAMLs))
	if err != nil {
		return err
	}
	for _, r := range resources {
		err = updateResourceConfiguration(r, vz)
		if err != nil {
			return err
		}
	}

	return k8s.ApplyResources(r.Clientset, r.RestConfig, resources, namespace, nil, false)
}

// deployVizierConfigs deploys the secrets, configmaps, and certs that are necessary for running vizier.
func (r *VizierReconciler) deployVizierConfigs(ctx context.Context, namespace string, vz *v1alpha1.Vizier, yamlMap map[string]string) error {
	log.Info("Deploying Vizier configs and secrets")
	resources, err := k8s.GetResourcesFromYAML(strings.NewReader(yamlMap["secrets"]))
	if err != nil {
		return err
	}
	for _, r := range resources {
		err = updateResourceConfiguration(r, vz)
		if err != nil {
			return err
		}
	}
	return k8s.ApplyResources(r.Clientset, r.RestConfig, resources, namespace, nil, false)
}

func (r *VizierReconciler) replaceOperatorManagedNATS(ctx context.Context, namespace string, vz *v1alpha1.Vizier, yamlMap map[string]string) error {
	log.Info("Checking for NATS")
	nc, err := nats.NewForConfig(r.RestConfig)
	if err != nil {
		// This probably means there's no NATS CRD installed. noop
		log.Info("Couldn't create NATS client")
		return nil
	}
	nats, err := nc.NatsV1alpha2().NatsClusters(namespace).Get(ctx, "pl-nats", metav1.GetOptions{})
	if err != nil {
		// This means that there's no operator managed NATSCluster. noop
		return nil
	}
	log.WithField("nats", nats).Info("Found natscluster, removing")

	// Watcher to watch for existing services to be cleaned up. We need to ensure that this occurs before
	// new statefulset is deployed.
	w, err := r.Clientset.CoreV1().Services(namespace).Watch(ctx, metav1.ListOptions{})
	if err != nil {
		return err
	}
	var wg sync.WaitGroup
	wg.Add(1)
	go func() {
		defer wg.Done()
		natsSeen := false
		natsMgmtSeen := false
		for r := range w.ResultChan() {
			if r.Type == watch.Deleted {
				svc, ok := r.Object.(*v1.Service)
				if !ok {
					continue
				}
				if svc.Name == "pl-nats" {
					natsSeen = true
				}
				if svc.Name == "pl-nats-mgmt" {
					natsMgmtSeen = true
				}
				if natsSeen && natsMgmtSeen {
					break
				}
			}
		}
	}()

	err = nc.NatsV1alpha2().NatsClusters(namespace).Delete(ctx, "pl-nats", metav1.DeleteOptions{})
	if err != nil {
		return err
	}
	// Wait for the existing services to be deleted.
	wg.Wait()
	w.Stop()
	return r.deployNATSStatefulset(ctx, namespace, vz, yamlMap)
}

func (r *VizierReconciler) replaceOperatorManagedEtcd(ctx context.Context, namespace string, vz *v1alpha1.Vizier, yamlMap map[string]string) error {
	log.Info("Checking for etcd")
	ec, err := etcd.NewForConfig(r.RestConfig)
	if err != nil {
		// This probably means there's no etcd CRD installed. noop
		log.Info("Couldn't create etcd client")
		return nil
	}
	etcd, err := ec.EtcdV1beta2().EtcdClusters(namespace).Get(ctx, "pl-etcd", metav1.GetOptions{})
	if err != nil {
		// This means that there's no operator managed etcdCluster. noop
		return nil
	}
	log.WithField("etcd", etcd).Info("Found etcdcluster, removing")

	// Watcher to watch for existing services to be cleaned up. We need to ensure that this occurs before
	// new statefulset is deployed.
	w, err := r.Clientset.CoreV1().Services(namespace).Watch(ctx, metav1.ListOptions{})
	if err != nil {
		return err
	}
	var wg sync.WaitGroup
	wg.Add(1)
	go func() {
		defer wg.Done()
		etcdSeen := false
		etcdClientSeen := false
		for r := range w.ResultChan() {
			if r.Type == watch.Deleted {
				svc, ok := r.Object.(*v1.Service)
				if !ok {
					continue
				}
				if svc.Name == "pl-etcd" {
					etcdSeen = true
				}
				if svc.Name == "pl-etcd-client" {
					etcdClientSeen = true
				}
				if etcdSeen && etcdClientSeen {
					break
				}
			}
		}
	}()

	err = ec.EtcdV1beta2().EtcdClusters(namespace).Delete(ctx, "pl-etcd", metav1.DeleteOptions{})
	if err != nil {
		return err
	}
	// Wait for the existing services to be deleted.
	wg.Wait()
	w.Stop()

	return r.deployEtcdStatefulset(ctx, namespace, vz, yamlMap)
}

// deployNATSStatefulset deploys nats to the given namespace.
func (r *VizierReconciler) deployNATSStatefulset(ctx context.Context, namespace string, vz *v1alpha1.Vizier, yamlMap map[string]string) error {
	log.Info("Deploying NATS")
	resources, err := k8s.GetResourcesFromYAML(strings.NewReader(yamlMap["nats"]))
	if err != nil {
		return err
	}
	for _, r := range resources {
		err = updateResourceConfiguration(r, vz)
		if err != nil {
			return err
		}
	}
	return retryDeploy(r.Clientset, r.RestConfig, namespace, resources, false)
}

// deployEtcdStatefulset deploys etcd to the given namespace.
func (r *VizierReconciler) deployEtcdStatefulset(ctx context.Context, namespace string, vz *v1alpha1.Vizier, yamlMap map[string]string) error {
	log.Info("Deploying etcd")
	resources, err := k8s.GetResourcesFromYAML(strings.NewReader(yamlMap["etcd"]))
	if err != nil {
		return err
	}
	for _, r := range resources {
		err = updateResourceConfiguration(r, vz)
		if err != nil {
			return err
		}
	}
	return retryDeploy(r.Clientset, r.RestConfig, namespace, resources, false)
}

// deployVizierDeps deploys the vizier deps to the given namespace. This includes deploying deps like etcd and nats.
func (r *VizierReconciler) deployVizierDeps(ctx context.Context, namespace string, vz *v1alpha1.Vizier, yamlMap map[string]string) error {
	err := r.deployNATSStatefulset(ctx, namespace, vz, yamlMap)
	if err != nil {
		return err
	}

	if !vz.Spec.UseEtcdOperator {
		return nil
	}

	return r.deployEtcdStatefulset(ctx, namespace, vz, yamlMap)
}

// deployVizierCore deploys the core pods and services for running vizier.
func (r *VizierReconciler) deployVizierCore(ctx context.Context, namespace string, vz *v1alpha1.Vizier, yamlMap map[string]string, allowUpdate bool) error {
	log.Info("Deploying Vizier")

	vzYaml := "vizier_persistent"
	if vz.Spec.UseEtcdOperator {
		vzYaml = "vizier_etcd"
	}

	resources, err := k8s.GetResourcesFromYAML(strings.NewReader(yamlMap[vzYaml]))
	if err != nil {
		return err
	}

	// If updating, don't reapply service accounts as that will create duplicate service tokens.
	if allowUpdate {
		filteredResources := make([]*k8s.Resource, 0)
		for _, r := range resources {
			if r.GVK.Kind != "ServiceAccount" {
				filteredResources = append(filteredResources, r)
			}
		}
		resources = filteredResources
	}

	for _, r := range resources {
		err = updateResourceConfiguration(r, vz)
		if err != nil {
			return err
		}
	}
	err = retryDeploy(r.Clientset, r.RestConfig, namespace, resources, allowUpdate)
	if err != nil {
		return err
	}

	return nil
}

func updateResourceConfiguration(resource *k8s.Resource, vz *v1alpha1.Vizier) error {
	// Add custom labels and annotations to the k8s resource.
	addKeyValueMapToResource("labels", vz.Spec.Pod.Labels, resource.Object.Object)
	addKeyValueMapToResource("annotations", vz.Spec.Pod.Annotations, resource.Object.Object)
	updateResourceRequirements(vz.Spec.Pod.Resources, resource.Object.Object)
	updatePodSpec(vz.Spec.Pod.NodeSelector, vz.Spec.Pod.SecurityContext, resource.Object.Object)
	return nil
}

func convertResourceType(originalLst v1.ResourceList) *vizierconfigpb.ResourceList {
	transformedList := make(map[string]*vizierconfigpb.ResourceQuantity)
	for rName, rQuantity := range originalLst {
		transformedList[string(rName)] = &vizierconfigpb.ResourceQuantity{
			Value: rQuantity.String(),
		}
	}
	return &vizierconfigpb.ResourceList{
		ResourceList: transformedList,
	}
}

// generateVizierYAMLsConfig is responsible retrieving a yaml map of configurations from
// Pixie Cloud.
func generateVizierYAMLsConfig(ctx context.Context, ns string, vz *v1alpha1.Vizier, conn *grpc.ClientConn) (*cloudpb.ConfigForVizierResponse,
	error) {
	client := cloudpb.NewConfigServiceClient(conn)

	req := &cloudpb.ConfigForVizierRequest{
		Namespace: ns,
		VzSpec: &vizierconfigpb.VizierSpec{
			Version:           vz.Spec.Version,
			DeployKey:         vz.Spec.DeployKey,
			DisableAutoUpdate: vz.Spec.DisableAutoUpdate,
			UseEtcdOperator:   vz.Spec.UseEtcdOperator,
			ClusterName:       vz.Spec.ClusterName,
			CloudAddr:         vz.Spec.CloudAddr,
			DevCloudNamespace: vz.Spec.DevCloudNamespace,
			PemMemoryLimit:    vz.Spec.PemMemoryLimit,
			ClockConverter:    string(vz.Spec.ClockConverter),
			DataAccess:        string(vz.Spec.DataAccess),
			Pod_Policy: &vizierconfigpb.PodPolicyReq{
				Labels:      vz.Spec.Pod.Labels,
				Annotations: vz.Spec.Pod.Annotations,
				Resources: &vizierconfigpb.ResourceReqs{
					Limits:   convertResourceType(vz.Spec.Pod.Resources.Limits),
					Requests: convertResourceType(vz.Spec.Pod.Resources.Requests),
				},
				NodeSelector: vz.Spec.Pod.NodeSelector,
			},
			Patches: vz.Spec.Patches,
		},
	}

	if vz.Spec.DataCollectorParams != nil {
		req.VzSpec.DataCollectorParams = &vizierconfigpb.DataCollectorParams{
			DatastreamBufferSize:      vz.Spec.DataCollectorParams.DatastreamBufferSize,
			DatastreamBufferSpikeSize: vz.Spec.DataCollectorParams.DatastreamBufferSpikeSize,
			TableStoreTableSizeLimit:  vz.Spec.DataCollectorParams.TableStoreTableSizeLimit,
			CustomPEMFlags:            vz.Spec.DataCollectorParams.CustomPEMFlags,
		}
	}

	if vz.Spec.LeadershipElectionParams != nil {
		req.VzSpec.LeadershipElectionParams = &vizierconfigpb.LeadershipElectionParams{
			ElectionPeriodMs: vz.Spec.LeadershipElectionParams.ElectionPeriodMs,
		}
	}

	resp, err := client.GetConfigForVizier(ctx, req)
	if err != nil {
		return nil, err
	}
	return resp, nil
}

// addKeyValueMapToResource adds the given keyValue map to the K8s resource.
func addKeyValueMapToResource(mapName string, keyValues map[string]string, res map[string]interface{}) {
	metadata := make(map[string]interface{})
	md, ok, err := unstructured.NestedFieldNoCopy(res, "metadata")
	if ok && err == nil {
		if mdCast, castOk := md.(map[string]interface{}); castOk {
			metadata = mdCast
		}
	}

	resLabels := make(map[string]interface{})
	l, ok, err := unstructured.NestedFieldNoCopy(res, "metadata", mapName)
	if ok && err == nil {
		if labelsCast, castOk := l.(map[string]interface{}); castOk {
			resLabels = labelsCast
		}
	}

	for k, v := range keyValues {
		resLabels[k] = v
	}
	metadata[mapName] = resLabels

	// If it exists, recursively add the labels to the resource's template (for deployments/daemonsets).
	spec, ok, err := unstructured.NestedFieldNoCopy(res, "spec", "template")
	if ok && err == nil {
		if specCast, castOk := spec.(map[string]interface{}); castOk {
			addKeyValueMapToResource(mapName, keyValues, specCast)
		}
	}

	res["metadata"] = metadata
}

func updateResourceRequirements(requirements v1.ResourceRequirements, res map[string]interface{}) {
	// Traverse through resource object to spec.template.spec.containers. If the path does not exist,
	// the resource can be ignored.

	containers, ok, err := unstructured.NestedFieldNoCopy(res, "spec", "template", "spec", "containers")
	if !ok || err != nil {
		return
	}

	cList, ok := containers.([]interface{})
	if !ok {
		return
	}

	// If containers are specified in the spec, we should update the resource requirements if
	// not already defined.
	for _, c := range cList {
		castedContainer, ok := c.(map[string]interface{})
		if !ok {
			continue
		}

		resources := make(map[string]interface{})
		if r, ok := castedContainer["resources"]; ok {
			castedR, castOk := r.(map[string]interface{})
			if castOk {
				resources = castedR
			}
		}

		requests := make(map[string]interface{})
		if req, ok := resources["requests"]; ok {
			castedReq, ok := req.(map[string]interface{})
			if ok {
				requests = castedReq
			}
		}
		for k, v := range requirements.Requests {
			if _, ok := requests[k.String()]; ok {
				continue
			}

			requests[k.String()] = v.String()
		}
		resources["requests"] = requests

		limits := make(map[string]interface{})
		if req, ok := resources["limits"]; ok {
			castedLim, ok := req.(map[string]interface{})
			if ok {
				limits = castedLim
			}
		}
		for k, v := range requirements.Limits {
			if _, ok := limits[k.String()]; ok {
				continue
			}

			limits[k.String()] = v.String()
		}
		resources["limits"] = limits

		castedContainer["resources"] = resources
	}
}
func updatePodSpec(nodeSelector map[string]string, securityCtx *v1alpha1.PodSecurityContext, res map[string]interface{}) {
	podSpec := make(map[string]interface{})
	md, ok, err := unstructured.NestedFieldNoCopy(res, "spec", "template", "spec")
	if ok && err == nil {
		if podSpecCast, castOk := md.(map[string]interface{}); castOk {
			podSpec = podSpecCast
		}
	}

	castedNodeSelector := make(map[string]interface{})
	for k, v := range nodeSelector {
		if _, ok := castedNodeSelector[k]; ok {
			continue
		}
		castedNodeSelector[k] = v
	}
	podSpec["nodeSelector"] = nodeSelector

	// Add securityContext only if enabled.
	if securityCtx == nil || !securityCtx.Enabled {
		return
	}
	sc, ok, err := unstructured.NestedFieldNoCopy(res, "spec", "template", "spec", "securityContext")
	if ok && err == nil {
		if scCast, castOk := sc.(map[string]interface{}); castOk && len(scCast) > 0 {
			return // A security context is already specified, we should use that one.
		}
	}

	sCtx := make(map[string]interface{})
	if securityCtx.FSGroup != 0 {
		sCtx["fsGroup"] = securityCtx.FSGroup
	}
	if securityCtx.RunAsUser != 0 {
		sCtx["runAsUser"] = securityCtx.RunAsUser
	}
	if securityCtx.RunAsGroup != 0 {
		sCtx["runAsGroup"] = securityCtx.RunAsGroup
	}

	podSpec["securityContext"] = sCtx
}

func waitForCluster(clientset *kubernetes.Clientset, namespace string) error {
	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Minute)
	defer cancel()
	t := time.NewTicker(2 * time.Second)
	defer t.Stop()

	clusterID := false
	for !clusterID { // Wait for secret to be updated with clusterID.
		select {
		case <-ctx.Done():
			return errors.New("Timed out waiting for cluster ID")
		case <-t.C:
			s := k8s.GetSecret(clientset, namespace, "pl-cluster-secrets")
			if s == nil {
				return errors.New("Missing cluster secrets")
			}
			if _, ok := s.Data["cluster-id"]; ok {
				clusterID = true
			}
		}
	}

	return nil
}

// watchForFailedVizierUpdates regularly polls for timed-out viziers
// and marks matching Viziers ReconciliationPhases as failed.
func (r *VizierReconciler) watchForFailedVizierUpdates() {
	t := time.NewTicker(updatingVizierCheckPeriod)
	defer t.Stop()
	for range t.C {
		var viziersList v1alpha1.VizierList
		ctx := context.Background()
		err := r.List(ctx, &viziersList)
		if err != nil {
			log.WithError(err).Error("Unable to list the vizier objects")
			continue
		}
		for _, vz := range viziersList.Items {
			// Set the Vizier Reconciliation phase to Failed if an Update has timed out.
			if vz.Status.ReconciliationPhase != v1alpha1.ReconciliationPhaseUpdating {
				continue
			}
			if time.Since(vz.Status.LastReconciliationPhaseTime.Time) < updatingFailedTimeout {
				continue
			}
			err := r.Status().Update(ctx, setReconciliationPhase(&vz, v1alpha1.ReconciliationPhaseFailed))
			if err != nil {
				log.WithError(err).Error("Unable to update vizier status")
			}
		}
	}
}

// SetupWithManager sets up the reconciler.
func (r *VizierReconciler) SetupWithManager(mgr ctrl.Manager) error {
	go r.watchForFailedVizierUpdates()
	return ctrl.NewControllerManagedBy(mgr).
		For(&v1alpha1.Vizier{}).
		Complete(r)
}

func retryDeploy(clientset *kubernetes.Clientset, config *rest.Config, namespace string, resources []*k8s.Resource, allowUpdate bool) error {
	bOpts := backoff.NewExponentialBackOff()
	bOpts.InitialInterval = 15 * time.Second
	bOpts.MaxElapsedTime = 5 * time.Minute

	return backoff.Retry(func() error {
		return k8s.ApplyResources(clientset, config, resources, namespace, nil, allowUpdate)
	}, bOpts)
}
