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
	"bytes"
	"context"
	"crypto/rand"
	"crypto/sha256"
	"encoding/json"
	"errors"
	"fmt"
	"strings"
	"time"

	"github.com/blang/semver"
	"github.com/cenkalti/backoff/v4"
	"github.com/gogo/protobuf/types"
	log "github.com/sirupsen/logrus"
	"google.golang.org/grpc"
	appsv1 "k8s.io/api/apps/v1"
	v1 "k8s.io/api/core/v1"
	storagev1 "k8s.io/api/storage/v1"
	k8serrors "k8s.io/apimachinery/pkg/api/errors"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/apimachinery/pkg/apis/meta/v1/unstructured"
	"k8s.io/apimachinery/pkg/runtime"
	"k8s.io/client-go/kubernetes"
	"k8s.io/client-go/rest"
	ctrl "sigs.k8s.io/controller-runtime"
	"sigs.k8s.io/controller-runtime/pkg/client"

	"px.dev/pixie/src/api/proto/cloudpb"
	"px.dev/pixie/src/api/proto/uuidpb"
	"px.dev/pixie/src/api/proto/vizierconfigpb"
	"px.dev/pixie/src/operator/apis/px.dev/v1alpha1"
	version "px.dev/pixie/src/shared/goversion"
	"px.dev/pixie/src/shared/services"
	"px.dev/pixie/src/shared/status"
	"px.dev/pixie/src/utils"
	"px.dev/pixie/src/utils/shared/certs"
	"px.dev/pixie/src/utils/shared/k8s"
)

const (
	// This is the key for the annotation that the operator applies on all of its deployed resources for a CRD.
	operatorAnnotation  = "vizier-name"
	clusterSecretJWTKey = "jwt-signing-key"
	// updatingFailedTimeout is the amount of time we wait since an Updated started
	// before we consider the Update Failed.
	updatingFailedTimeout = 10 * time.Minute
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

	monitor      *VizierMonitor
	lastChecksum []byte
	K8sVersion   string

	sentryFlush func()
}

// +kubebuilder:rbac:groups=pixie.px.dev,resources=viziers,verbs=get;list;watch;create;update;patch;delete
// +kubebuilder:rbac:groups=pixie.px.dev,resources=viziers/status,verbs=get;update;patch

func getCloudClientConnection(cloudAddr string, devCloudNS string, extraDialOpts ...grpc.DialOption) (*grpc.ClientConn, error) {
	isInternal := false

	if devCloudNS != "" {
		cloudAddr = fmt.Sprintf("api-service.%s.svc.cluster.local:51200", devCloudNS)
		isInternal = true
	}

	dialOpts, err := services.GetGRPCClientDialOptsServerSideTLS(isInternal)
	dialOpts = append(dialOpts, extraDialOpts...)
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

// Kubernetes used to have in-tree plugins for a variety of vendor CSI drivers.
// These provisioners had "kubernetes.io/" prefixes. Often times these provisioner names
// are still used in the storageclass but the calls are redirected to CSIDrivers under new names.
// This map maintains that list of redirects.
// See https://kubernetes.io/docs/concepts/storage/volumes and
// https://kubernetes.io/docs/concepts/storage/storage-classes/#provisioner.
var migratedCSIDrivers = map[string]string{
	"kubernetes.io/aws-ebs":         "ebs.csi.aws.com",
	"kubernetes.io/azure-disk":      "disk.csi.azure.com",
	"kubernetes.io/azure-file":      "file.csi.azure.com",
	"kubernetes.io/cinder":          "cinder.csi.openstack.org",
	"kubernetes.io/gce-pd":          "pd.csi.storage.gke.io",
	"kubernetes.io/portworx-volume": "pxd.portworx.com",
	"kubernetes.io/vsphere-volume":  "csi.vsphere.vmware.com",
}

func hasCSIDriver(clientset *kubernetes.Clientset, name string) (bool, error) {
	_, err := clientset.StorageV1().CSIDrivers().Get(context.Background(), name, metav1.GetOptions{})
	if err != nil && !k8serrors.IsNotFound(err) {
		return false, err
	} else if k8serrors.IsNotFound(err) {
		return false, nil
	}
	return true, nil
}

func defaultStorageClassHasCSIDriver(clientset *kubernetes.Clientset) (bool, error) {
	storageClasses, err := clientset.StorageV1().StorageClasses().List(context.Background(), metav1.ListOptions{})
	if err != nil {
		return false, err
	}

	var defaultStorageClass *storagev1.StorageClass
	// Check annotations map on each storage class to see if default is set to "true".
	for _, storageClass := range storageClasses.Items {
		annotationsMap := storageClass.GetAnnotations()
		for _, key := range defaultClassAnnotationKeys {
			if annotationsMap[key] == "true" {
				defaultStorageClass = &storageClass
				break
			}
		}
	}
	if defaultStorageClass == nil {
		return false, errors.New("no default storage class")
	}
	csi := defaultStorageClass.Provisioner
	if migrated, ok := migratedCSIDrivers[csi]; ok {
		csi = migrated
	}

	// For all non-migrated kubernetes provisioners, kubernetes itself will have an internal provisioner,
	// so no need to check for a CSIDriver.
	if strings.HasPrefix(csi, "kubernetes.io/") {
		return true, nil
	}
	// If the provisioner contains a "/" then we assume it is a custom provisioner that doesn't use the CSI pattern,
	// so we skip checking for a CSIDriver and hope the provisioner works.
	if strings.Contains(csi, "/") {
		return true, nil
	}

	return hasCSIDriver(clientset, csi)
}

// missingNecessaryCSIDriver checks if the user is running an EKS cluster, and if so, whether they are
// missing the CSIDriver. Without the CSI driver, persistent volumes may not be able to be deployed.
func missingNecessaryCSIDriver(clientset *kubernetes.Clientset, k8sVersion string) bool {
	// This check only needs to be done for eks clusters with K8s version > 1.22.0.
	if !strings.Contains(k8sVersion, "-eks-") {
		return false
	}

	parsedVersion, err := semver.ParseTolerant(k8sVersion)
	if err != nil {
		log.WithError(err).Error("Failed to parse K8s cluster version")
		return false
	}
	driverVersionRange, _ := semver.ParseRange("<=1.22.0")
	if driverVersionRange(parsedVersion) {
		return false
	}

	hasDriver, err := defaultStorageClassHasCSIDriver(clientset)
	if err != nil {
		log.WithError(err).Warn("failed to determine if the default storage class has a valid CSI driver")
		return false
	}
	return !hasDriver
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
	log.WithField("req", req).Info("Reconciling Vizier...")
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

	// Check if vizier already exists, if not create a new vizier.
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
	if r.monitor == nil || r.monitor.namespace != req.Namespace || r.monitor.devCloudNamespace != vizier.Spec.DevCloudNamespace {
		if r.monitor != nil {
			r.monitor.Quit()
			r.monitor = nil
		}

		r.monitor = &VizierMonitor{
			namespace:         req.Namespace,
			namespacedName:    req.NamespacedName,
			devCloudNamespace: vizier.Spec.DevCloudNamespace,
			vzUpdate:          r.Status().Update,
			vzGet:             r.Get,
			clientset:         r.Clientset,
			vzSpecUpdate:      r.Update,
			restConfig:        r.RestConfig,
		}

		cloudClient, err := getCloudClientConnection(vizier.Spec.CloudAddr, vizier.Spec.DevCloudNamespace, grpc.FailOnNonTempDialError(true), grpc.WithBlock())
		if err != nil {
			vizier.SetStatus(status.UnableToConnectToCloud)
			err := r.Status().Update(ctx, &vizier)
			if err != nil {
				if strings.Contains(err.Error(), "timeout") {
					log.WithError(err).Info("Timed out trying to update vizier status. K8s API server may be overloaded")
				} else {
					log.WithError(err).Error("Failed to update vizier status")
				}
			}
			log.WithError(err).Error("Failed to connect to Pixie cloud")
			return ctrl.Result{}, err
		}

		if r.sentryFlush == nil {
			r.sentryFlush = setupSentry(ctx, cloudClient, r.Clientset)
		}

		r.monitor.InitAndStartMonitor(cloudClient)

		// Update operator version
		vizier.Status.OperatorVersion = version.GetVersion().ToString()
		err = r.Status().Update(ctx, &vizier)
		if err != nil {
			if strings.Contains(err.Error(), "timeout") {
				log.WithError(err).Info("Timed out trying to update vizier status. K8s API server may be overloaded")
			} else {
				log.WithError(err).Error("Failed to update vizier status")
			}
		}
	}

	// Vizier CRD has been updated, and we should update the running vizier accordingly.
	return ctrl.Result{}, err
}

// updateVizier updates the vizier instance according to the spec.
func (r *VizierReconciler) updateVizier(ctx context.Context, req ctrl.Request, vz *v1alpha1.Vizier) error {
	log.Info("Updating Vizier...")

	checksum, err := getSpecChecksum(vz)
	if err != nil {
		return err
	}

	if bytes.Equal(checksum, vz.Status.Checksum) {
		log.Info("Checksums matched, no need to reconcile")
		return nil
	}

	if len(vz.Status.Checksum) == 0 && bytes.Equal(checksum, r.lastChecksum) {
		log.Warn("No checksum written to status")
		log.Info("Checksums matched, no need to reconcile")
		return nil
	}

	if vz.Status.ReconciliationPhase == v1alpha1.ReconciliationPhaseUpdating {
		log.Info("Already in the process of updating, nothing to do")
		return nil
	}
	log.Infof("Status checksum '%x' does not match spec checksum '%x' - running an update", vz.Status.Checksum, checksum)

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
	return nil
}

// createVizier deploys a new vizier instance in the given namespace.
func (r *VizierReconciler) createVizier(ctx context.Context, req ctrl.Request, vz *v1alpha1.Vizier) error {
	log.Info("Creating a new vizier instance")
	cloudClient, err := getCloudClientConnection(vz.Spec.CloudAddr, vz.Spec.DevCloudNamespace)
	if err != nil {
		vz.SetStatus(status.UnableToConnectToCloud)
		err := r.Status().Update(ctx, vz)
		if err != nil {
			if strings.Contains(err.Error(), "timeout") {
				log.WithError(err).Info("Timed out trying to update vizier status. K8s API server may be overloaded")
			} else {
				log.WithError(err).Error("Failed to update vizier status")
			}
		}
		log.WithError(err).Error("Failed to connect to Pixie cloud")
		return err
	}

	// If no version is set, we should fetch the latest version. This will trigger another reconcile that will do
	// the actual vizier deployment.
	if vz.Spec.Version == "" {
		atClient := cloudpb.NewArtifactTrackerClient(cloudClient)
		latest, err := getLatestVizierVersion(ctx, atClient)
		if err != nil {
			log.WithError(err).Error("Failed to get latest Vizier version")
			return err
		}
		vz.Spec.Version = latest
		err = r.Update(ctx, vz)
		if err != nil {
			log.WithError(err).Error("Failed to update version in Vizier spec")
			return err
		}
		return nil
	}

	return r.deployVizier(ctx, req, vz, false)
}

func (r *VizierReconciler) deployVizier(ctx context.Context, req ctrl.Request, vz *v1alpha1.Vizier, update bool) error {
	log.Info("Starting a vizier deploy")
	cloudClient, err := getCloudClientConnection(vz.Spec.CloudAddr, vz.Spec.DevCloudNamespace)
	if err != nil {
		vz.SetStatus(status.UnableToConnectToCloud)
		err := r.Status().Update(ctx, vz)
		if err != nil {
			if strings.Contains(err.Error(), "timeout") {
				log.WithError(err).Info("Timed out trying to update vizier status. K8s API server may be overloaded")
			} else {
				log.WithError(err).Error("Failed to update vizier status")
			}
		}
		log.WithError(err).Error("Failed to connect to Pixie cloud")
		return err
	}

	// Set the status of the Vizier.
	vz.SetReconciliationPhase(v1alpha1.ReconciliationPhaseUpdating)
	err = r.Status().Update(ctx, vz)
	if err != nil {
		log.WithError(err).Error("Failed to update status in Vizier spec")
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

	if !vz.Spec.UseEtcdOperator && !update {
		// Check if the cluster offers PVC support.
		// If it does not, we should default to using the etcd operator, which does not
		// require PVC support.
		defaultStorageExists, err := validateNumDefaultStorageClasses(r.Clientset)
		if err != nil {
			log.WithError(err).Error("Error checking default storage classes")
		}
		missingCSIDriver := missingNecessaryCSIDriver(r.Clientset, r.K8sVersion)

		if !defaultStorageExists || missingCSIDriver {
			log.Warn("No default storage class detected for cluster. Deploying etcd operator instead of statefulset for metadata backend.")
			vz.Spec.UseEtcdOperator = true
		}
	}

	vz.Spec.Pod.Annotations[operatorAnnotation] = req.Name
	vz.Spec.Pod.Labels[operatorAnnotation] = req.Name

	// Update the spec in the k8s api as other parts of the code expect this to be true.
	err = r.Update(ctx, vz)
	if err != nil {
		log.WithError(err).Error("Failed to update spec for Vizier CRD")
		return err
	}

	// Get the checksum up here in case the spec changes midway through.
	checksum, err := getSpecChecksum(vz)
	if err != nil {
		return err
	}

	// Get the Vizier's ID from the cluster's secrets.
	vizierID, err := getVizierID(r.Clientset, req.Namespace)
	if err != nil {
		log.WithError(err).Error("Failed to retrieve the Vizier ID from the cluster's secrets")
	}

	configForVizierResp, err := generateVizierYAMLsConfig(ctx, req.Namespace, r.K8sVersion, vizierID, vz, cloudClient)
	if err != nil {
		log.WithError(err).Error("Failed to generate configs for Vizier YAMLs")
		return err
	}
	yamlMap := configForVizierResp.NameToYamlContent

	// Update Vizier CRD status sentryDSN so that it can be accessed by other
	// vizier pods.
	vz.Status.SentryDSN = configForVizierResp.SentryDSN

	if !update {
		err = r.deployVizierConfigs(ctx, req.Namespace, vz, yamlMap)
		if err != nil {
			log.WithError(err).Error("Failed to deploy Vizier configs")
			return err
		}

		err = r.deployVizierCerts(ctx, req.Namespace, vz)
		if err != nil {
			log.WithError(err).Error("Failed to deploy Vizier certs")
			return err
		}

		err = r.deployNATSStatefulset(ctx, req.Namespace, vz, yamlMap)
		if err != nil {
			log.WithError(err).Error("Failed to deploy NATS")
			return err
		}
	} else {
		err = r.upgradeNats(ctx, req.Namespace, vz, yamlMap)
		if err != nil {
			log.WithError(err).Warning("Failed to upgrade nats")
		}
	}

	if vz.Spec.UseEtcdOperator {
		err := r.Clientset.AppsV1().StatefulSets(req.Namespace).Delete(ctx, "vizier-metadata", metav1.DeleteOptions{})
		if err != nil && k8serrors.IsNotFound(err) {
			log.Debug("vizier-metadata statefulset not found, skipping deletion")
		} else if err != nil {
			log.WithError(err).Error("Failed to delete vizier-metadata statefulset")
			return err
		} else {
			log.Info("Deleted vizier-metadata statefulset")
		}
		err = r.deployEtcdStatefulset(ctx, req.Namespace, vz, yamlMap)
		if err != nil {
			log.WithError(err).Error("Failed to deploy etcd")
			return err
		}
	} else {
		// Delete the etcd statefulset if it exists.
		err := r.Clientset.AppsV1().StatefulSets(req.Namespace).Delete(ctx, "pl-etcd", metav1.DeleteOptions{})
		if err != nil && k8serrors.IsNotFound(err) {
			log.Debug("pl-etcd statefulset not found, skipping deletion")
		} else if err != nil {
			log.WithError(err).Error("Failed to delete pl-etcd statefulset")
			return err
		} else {
			log.Info("Deleted pl-etcd statefulset")
		}
		err = r.Clientset.AppsV1().Deployments(req.Namespace).Delete(ctx, "vizier-metadata", metav1.DeleteOptions{})
		if err != nil && k8serrors.IsNotFound(err) {
			log.Debug("vizier-metadata deployment not found, skipping deletion")
		} else if err != nil {
			log.WithError(err).Error("Failed to delete metadata deployment")
			return err
		} else {
			log.Info("Deleted vizier-metadata deployment")
		}
	}
	err = r.deployVizierCore(ctx, req.Namespace, vz, yamlMap, update)
	if err != nil {
		log.WithError(err).Info("Failed to deploy Vizier core")
		return err
	}

	// TODO(michellenguyen): Remove when the operator has the ability to ping CloudConn for Vizier Version.
	// We are currently blindly assuming that the new version is correct.
	_ = waitForCluster(r.Clientset, req.Namespace)

	// Refetch the Vizier resource, as it may have changed in the time in which we were waiting for the cluster.
	err = r.Get(ctx, req.NamespacedName, vz)
	if err != nil {
		log.WithError(err).Info("Failed to get vizier after deploy. Vizier was likely deleted")
		// The Vizier was deleted in the meantime. Do nothing.
		return nil
	}

	vz.Status.Version = vz.Spec.Version
	vz.SetReconciliationPhase(v1alpha1.ReconciliationPhaseReady)

	vz.Status.Checksum = checksum
	r.lastChecksum = checksum
	err = r.Status().Update(ctx, vz)
	if err != nil {
		return err
	}

	log.Info("Vizier deploy is complete")
	return nil
}

func getSpecChecksum(vz *v1alpha1.Vizier) ([]byte, error) {
	specStr, err := json.Marshal(vz.Spec)
	if err != nil {
		log.WithError(err).Info("Failed to marshal spec to JSON")
		return nil, err
	}
	h := sha256.New()
	h.Write([]byte(specStr))
	return h.Sum(nil), nil
}

func (r *VizierReconciler) upgradeNats(ctx context.Context, namespace string, vz *v1alpha1.Vizier, yamlMap map[string]string) error {
	log.Info("Upgrading NATS if necessary")

	ss, err := r.Clientset.AppsV1().StatefulSets(namespace).Get(ctx, "pl-nats", metav1.GetOptions{})
	if err != nil {
		log.WithError(err).Info("No NATS currently running")
		return r.deployNATSStatefulset(ctx, namespace, vz, yamlMap)
	}

	containers := ss.Spec.Template.Spec.Containers
	if len(containers) == 0 {
		log.Info("NATS seems to have no containers")
		return r.deployNATSStatefulset(ctx, namespace, vz, yamlMap)
	}
	natsImage := containers[0].Image

	resources, err := k8s.GetResourcesFromYAML(strings.NewReader(yamlMap["nats"]))
	if err != nil {
		return err
	}

	var newSS appsv1.StatefulSet
	for _, r := range resources {
		if r.GVK.Kind != "StatefulSet" {
			continue
		}
		err = runtime.DefaultUnstructuredConverter.FromUnstructured(r.Object.UnstructuredContent(), &newSS)
		if err != nil {
			log.WithError(err).Info("Could not decode NATS Statefulset")
			return err
		}
		break
	}

	if len(newSS.Spec.Template.Spec.Containers) == 0 {
		log.Info("New NATS spec seems to have no containers")
		return r.deployNATSStatefulset(ctx, namespace, vz, yamlMap)
	}

	if natsImage == newSS.Spec.Template.Spec.Containers[0].Image {
		log.Info("NATS up to date. Nothing to do.")
		return nil
	}

	log.Info("Will upgrade NATS")
	return r.deployNATSStatefulset(ctx, namespace, vz, yamlMap)
}

func (r *VizierReconciler) deployVizierCerts(ctx context.Context, namespace string, vz *v1alpha1.Vizier) error {
	return deployCerts(ctx, namespace, vz, r.Clientset, r.RestConfig, false)
}

func deployCerts(ctx context.Context, namespace string, vz *v1alpha1.Vizier, clientset kubernetes.Interface, restConfig *rest.Config, update bool) error {
	log.Info("Generating certs")

	// Assign JWT signing key.
	jwtSigningKey := make([]byte, 64)
	_, err := rand.Read(jwtSigningKey)
	if err != nil {
		return err
	}
	s := k8s.GetSecret(clientset, namespace, "pl-cluster-secrets")
	if s == nil {
		return errors.New("pl-cluster-secrets does not exist")
	}
	s.Data[clusterSecretJWTKey] = []byte(fmt.Sprintf("%x", jwtSigningKey))

	_, err = clientset.CoreV1().Secrets(namespace).Update(ctx, s, metav1.UpdateOptions{})
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

	return k8s.ApplyResources(clientset, restConfig, resources, namespace, nil, update)
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
	return retryDeploy(r.Clientset, r.RestConfig, namespace, resources, true)
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

// deployVizierCore deploys the core pods and services for running vizier.
func (r *VizierReconciler) deployVizierCore(ctx context.Context, namespace string, vz *v1alpha1.Vizier, yamlMap map[string]string, allowUpdate bool) error {
	log.Info("Deploying Vizier")

	vzYaml := "vizier_persistent"
	if vz.Spec.UseEtcdOperator {
		vzYaml = "vizier_etcd"
	}

	if vz.Spec.Autopilot {
		vzYaml = fmt.Sprintf("%s_ap", vzYaml)
	}

	resources, err := k8s.GetResourcesFromYAML(strings.NewReader(yamlMap[vzYaml]))
	if err != nil {
		log.WithError(err).Error("Error getting resources from Vizier YAML")
		return err
	}

	for _, r := range resources {
		err = updateResourceConfiguration(r, vz)
		if err != nil {
			log.WithError(err).Error("Failed to update resource configuration for resources")
			return err
		}
	}
	err = retryDeploy(r.Clientset, r.RestConfig, namespace, resources, allowUpdate)
	if err != nil {
		log.WithError(err).Error("Retry deploy of Vizier failed")
		return err
	}

	return nil
}

func updateResourceConfiguration(resource *k8s.Resource, vz *v1alpha1.Vizier) error {
	// Add custom labels and annotations to the k8s resource.
	addKeyValueMapToResource("labels", vz.Spec.Pod.Labels, resource.Object.Object)
	addKeyValueMapToResource("annotations", vz.Spec.Pod.Annotations, resource.Object.Object)
	updateResourceRequirements(vz.Spec.Pod.Resources, resource.Object.Object)
	updatePodSpec(vz.Spec.Pod.NodeSelector, vz.Spec.Pod.Tolerations, vz.Spec.Pod.SecurityContext, resource.Object.Object)
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
func generateVizierYAMLsConfig(ctx context.Context, ns string, k8sVersion string, vizierID *uuidpb.UUID, vz *v1alpha1.Vizier, conn *grpc.ClientConn) (*cloudpb.ConfigForVizierResponse,
	error) {
	client := cloudpb.NewConfigServiceClient(conn)

	req := &cloudpb.ConfigForVizierRequest{
		Namespace:  ns,
		K8sVersion: k8sVersion,
		VizierID:   vizierID,
		VzSpec: &vizierconfigpb.VizierSpec{
			Version:               vz.Spec.Version,
			DeployKey:             vz.Spec.DeployKey,
			CustomDeployKeySecret: vz.Spec.CustomDeployKeySecret,
			DisableAutoUpdate:     vz.Spec.DisableAutoUpdate,
			UseEtcdOperator:       vz.Spec.UseEtcdOperator,
			ClusterName:           vz.Spec.ClusterName,
			CloudAddr:             vz.Spec.CloudAddr,
			DevCloudNamespace:     vz.Spec.DevCloudNamespace,
			PemMemoryLimit:        vz.Spec.PemMemoryLimit,
			PemMemoryRequest:      vz.Spec.PemMemoryRequest,
			ClockConverter:        string(vz.Spec.ClockConverter),
			DataAccess:            string(vz.Spec.DataAccess),
			Pod_Policy: &vizierconfigpb.PodPolicyReq{
				Labels:      vz.Spec.Pod.Labels,
				Annotations: vz.Spec.Pod.Annotations,
				Resources: &vizierconfigpb.ResourceReqs{
					Limits:   convertResourceType(vz.Spec.Pod.Resources.Limits),
					Requests: convertResourceType(vz.Spec.Pod.Resources.Requests),
				},
				NodeSelector: vz.Spec.Pod.NodeSelector,
				Tolerations:  convertTolerations(vz.Spec.Pod.Tolerations),
			},
			Patches:  vz.Spec.Patches,
			Registry: vz.Spec.Registry,
		},
	}

	if vz.Spec.DataCollectorParams != nil {
		req.VzSpec.DataCollectorParams = &vizierconfigpb.DataCollectorParams{
			DatastreamBufferSize:      vz.Spec.DataCollectorParams.DatastreamBufferSize,
			DatastreamBufferSpikeSize: vz.Spec.DataCollectorParams.DatastreamBufferSpikeSize,
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

func convertTolerations(tolerations []v1.Toleration) []*vizierconfigpb.Toleration {
	var castedTolerations []*vizierconfigpb.Toleration
	for _, toleration := range tolerations {
		castedToleration := &vizierconfigpb.Toleration{
			Key:      toleration.Key,
			Operator: string(toleration.Operator),
			Value:    toleration.Value,
			Effect:   string(toleration.Effect),
		}
		if toleration.TolerationSeconds != nil {
			castedToleration.TolerationSeconds = &types.Int64Value{Value: *toleration.TolerationSeconds}
		}
		castedTolerations = append(castedTolerations, castedToleration)
	}
	return castedTolerations
}

func updatePodSpec(nodeSelector map[string]string, tolerations []v1.Toleration, securityCtx *v1alpha1.PodSecurityContext, res map[string]interface{}) {
	podSpec := make(map[string]interface{})
	md, ok, err := unstructured.NestedFieldNoCopy(res, "spec", "template", "spec")
	if ok && err == nil {
		if podSpecCast, castOk := md.(map[string]interface{}); castOk {
			podSpec = podSpecCast
		}
	}

	castedNodeSelector := make(map[string]interface{})
	ns, ok := podSpec["nodeSelector"].(map[string]interface{})
	if ok {
		castedNodeSelector = ns
	}
	for k, v := range nodeSelector {
		if _, ok := castedNodeSelector[k]; ok {
			continue
		}
		castedNodeSelector[k] = v
	}
	podSpec["nodeSelector"] = castedNodeSelector
	podSpec["tolerations"] = tolerations

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
			log.WithField("namespace", vz.Namespace).WithField("vizier", vz.Name).Info("Marking vizier as failed")
			vz.SetReconciliationPhase(v1alpha1.ReconciliationPhaseFailed)
			err := r.Status().Update(ctx, &vz)
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

// Stop performs any necessary cleanup before shutdown.
func (r *VizierReconciler) Stop() {
	if r.sentryFlush != nil {
		r.sentryFlush()
	}
}

// setupSentry sets up the error logging.
func setupSentry(ctx context.Context, conn *grpc.ClientConn, clientset *kubernetes.Clientset) func() {
	// Use k8s UID instead of cluserID because newly deployed clusters may take some time to register and receive a clusterID
	clusterUID, err := getClusterUID(clientset)
	if err != nil {
		log.WithError(err).Error("Failed to get Cluster UID")
		return nil
	}

	config, err := getConfigForOperator(ctx, conn)
	if err != nil {
		log.WithError(err).Error("Failed to get Operator config")
		return nil
	}

	flush := services.InitSentryWithDSN(clusterUID, config.SentryOperatorDSN)
	return flush
}

// GetClusterUID gets UID for the cluster, represented by the kube-system namespace UID.
func getClusterUID(clientset *kubernetes.Clientset) (string, error) {
	ksNS, err := clientset.CoreV1().Namespaces().Get(context.Background(), "kube-system", metav1.GetOptions{})
	if err != nil {
		return "", err
	}
	return string(ksNS.UID), nil
}

// getVizierID gets the ID of the cluster the Vizier is in.
func getVizierID(clientset *kubernetes.Clientset, namespace string) (*uuidpb.UUID, error) {
	op := func() (*uuidpb.UUID, error) {
		var vizierID *uuidpb.UUID
		s := k8s.GetSecret(clientset, namespace, "pl-cluster-secrets")
		if s == nil {
			return nil, errors.New("Missing cluster secrets, retrying again")
		}
		if id, ok := s.Data["cluster-id"]; ok {
			vizierID = utils.ProtoFromUUIDStrOrNil(string(id))
			if vizierID == nil {
				return nil, errors.New("Couldn't convert ID to proto")
			}
		}

		return vizierID, nil
	}

	expBackoff := backoff.NewExponentialBackOff()
	expBackoff.InitialInterval = 10 * time.Second
	expBackoff.Multiplier = 2
	expBackoff.MaxElapsedTime = 10 * time.Minute

	vizierID, err := backoff.RetryWithData(op, expBackoff)
	if err != nil {
		return nil, errors.New("Timed out waiting for the Vizier ID")
	}

	return vizierID, nil
}

// getConfigForOperator is responsible retrieving the Operator config from from Pixie Cloud.
func getConfigForOperator(ctx context.Context, conn *grpc.ClientConn) (*cloudpb.ConfigForOperatorResponse, error) {
	client := cloudpb.NewConfigServiceClient(conn)
	req := &cloudpb.ConfigForOperatorRequest{}
	resp, err := client.GetConfigForOperator(ctx, req)
	if err != nil {
		return nil, err
	}
	return resp, nil
}

func retryDeploy(clientset *kubernetes.Clientset, config *rest.Config, namespace string, resources []*k8s.Resource, allowUpdate bool) error {
	bOpts := backoff.NewExponentialBackOff()
	bOpts.InitialInterval = 15 * time.Second
	bOpts.MaxElapsedTime = 5 * time.Minute

	return backoff.Retry(func() error {
		return k8s.ApplyResources(clientset, config, resources, namespace, nil, allowUpdate)
	}, bOpts)
}
