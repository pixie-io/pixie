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
	"time"

	"github.com/cenkalti/backoff/v3"
	log "github.com/sirupsen/logrus"
	"google.golang.org/grpc"
	v1 "k8s.io/api/core/v1"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/apimachinery/pkg/apis/meta/v1/unstructured"
	"k8s.io/apimachinery/pkg/runtime"
	"k8s.io/client-go/kubernetes"
	"k8s.io/client-go/rest"
	ctrl "sigs.k8s.io/controller-runtime"
	"sigs.k8s.io/controller-runtime/pkg/client"

	"px.dev/pixie/src/api/proto/cloudpb"
	pixiev1alpha1 "px.dev/pixie/src/operator/api/v1alpha1"
	"px.dev/pixie/src/shared/services"
	"px.dev/pixie/src/utils/shared/artifacts"
	"px.dev/pixie/src/utils/shared/certs"
	"px.dev/pixie/src/utils/shared/k8s"
	yamlsutils "px.dev/pixie/src/utils/shared/yamls"
	vizieryamls "px.dev/pixie/src/utils/template_generator/vizier_yamls"
)

// OperatorAnnotation is the key for the annotation that the operator applies on all of its deployed resources for a CRD.
const operatorAnnotation = "vizier-name"
const clusterSecretJWTKey = "jwt-signing-key"

// VizierReconciler reconciles a Vizier object
type VizierReconciler struct {
	client.Client
	Scheme *runtime.Scheme

	Clientset  *kubernetes.Clientset
	RestConfig *rest.Config
}

// +kubebuilder:rbac:groups=pixie.px.dev,resources=viziers,verbs=get;list;watch;create;update;patch;delete
// +kubebuilder:rbac:groups=pixie.px.dev,resources=viziers/status,verbs=get;update;patch

func getCloudClientConnection(cloudAddr string) (*grpc.ClientConn, error) {
	isInternal := strings.ContainsAny(cloudAddr, "cluster.local")

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

func getLatestVizierVersion(ctx context.Context, conn *grpc.ClientConn) (string, error) {
	client := cloudpb.NewArtifactTrackerClient(conn)

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

// Reconcile updates the Vizier running in the cluster to match the expected state.
func (r *VizierReconciler) Reconcile(ctx context.Context, req ctrl.Request) (ctrl.Result, error) {
	log.WithField("req", req).Info("Reconciling...")

	// Fetch vizier CRD to determine what operation should be performed.
	var vizier pixiev1alpha1.Vizier
	if err := r.Get(ctx, req.NamespacedName, &vizier); err != nil {
		// Vizier CRD deleted. The vizier instance should also be deleted.
		return ctrl.Result{}, r.deleteVizier(ctx, req)
	}

	if vizier.Status.VizierPhase == pixiev1alpha1.VizierPhaseNone {
		// We are creating a new vizier instance.
		err := r.createVizier(ctx, req, &vizier)
		if err != nil {
			log.WithError(err).Info("Failed to deploy new Vizier instance")
		}
		return ctrl.Result{}, err
	}

	// Vizier CRD has been updated, and we should update the running vizier accordingly.
	return ctrl.Result{}, r.updateVizier(ctx, req, &vizier)
}

// updateVizier updates the vizier instance according to the spec. As of the current moment, we only support updates to the Vizier version.
// Other updates to the Vizier spec will be ignored.
func (r *VizierReconciler) updateVizier(ctx context.Context, req ctrl.Request, vz *pixiev1alpha1.Vizier) error {
	// TODO: We currently only trigger updates on changing Vizier versions. We should add a webhook
	// to disallow changes to other fields.
	if vz.Status.Version == vz.Spec.Version {
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
	_, _ = od.DeleteByLabel(keyValueLabel, k8s.AllResourceKinds...)
	_, _ = od.DeleteByLabel("app=nats", k8s.AllResourceKinds...)
	_, _ = od.DeleteByLabel("etcd_cluster=pl-etcd", k8s.AllResourceKinds...)
	_ = od.DeleteCustomObject("NatsCluster", "pl-nats")
	return nil
}

// createVizier deploys a new vizier instance in the given namespace.
func (r *VizierReconciler) createVizier(ctx context.Context, req ctrl.Request, vz *pixiev1alpha1.Vizier) error {
	cloudClient, err := getCloudClientConnection(vz.Spec.CloudAddr)
	if err != nil {
		return err
	}

	// If no version is set, we should fetch the latest version. This will trigger another reconcile that will do
	// the actual vizier deployment.
	if vz.Spec.Version == "" {
		latest, err := getLatestVizierVersion(ctx, cloudClient)
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

func (r *VizierReconciler) deployVizier(ctx context.Context, req ctrl.Request, vz *pixiev1alpha1.Vizier, update bool) error {
	cloudClient, err := getCloudClientConnection(vz.Spec.CloudAddr)
	if err != nil {
		return err
	}

	// Set the status of the Vizier.
	vz.Status.VizierPhase = pixiev1alpha1.VizierPhasePending
	err = r.Status().Update(ctx, vz)
	if err != nil {
		return err
	}

	// Add an additional annotation to our deployed vizier-resources, to allow easier tracking of the vizier resources.
	if vz.Spec.Pod == nil {
		vz.Spec.Pod = &pixiev1alpha1.PodPolicy{}
	}

	if vz.Spec.Pod.Annotations == nil {
		vz.Spec.Pod.Annotations = make(map[string]string)
	}

	if vz.Spec.Pod.Labels == nil {
		vz.Spec.Pod.Labels = make(map[string]string)
	}

	vz.Spec.Pod.Annotations[operatorAnnotation] = req.Name
	vz.Spec.Pod.Labels[operatorAnnotation] = req.Name

	yamlMap, err := generateVizierYAMLs(vz.Spec.Version, req.Namespace, vz, cloudClient)
	if err != nil {
		return err
	}

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
	}

	err = r.deployVizierCore(ctx, req.Namespace, vz, yamlMap, update)
	if err != nil {
		return err
	}

	err = waitForCluster(r.Clientset, req.Namespace)
	vz.Status.Version = vz.Spec.Version
	if err != nil {
		log.WithError(err).Info("Failed healthcheck")
		vz.Status.VizierPhase = pixiev1alpha1.VizierPhaseFailed
		vz.Status.Message = err.Error()
	} else {
		vz.Status.VizierPhase = pixiev1alpha1.VizierPhaseRunning
	}

	err = r.Status().Update(ctx, vz)
	if err != nil {
		return err
	}

	return nil
}

// TODO(michellenguyen): Add a goroutine
// which checks when certs are about to expire. If they are about to expire,
// we should generate new certs and bounce all pods.
func (r *VizierReconciler) deployVizierCerts(ctx context.Context, namespace string, vz *pixiev1alpha1.Vizier) error {
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
func (r *VizierReconciler) deployVizierConfigs(ctx context.Context, namespace string, vz *pixiev1alpha1.Vizier, yamlMap map[string]string) error {
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

// deployVizierDeps deploys the vizier deps to the given namespace. This includes generating certs
// along with deploying deps like etcd and nats.
func (r *VizierReconciler) deployVizierDeps(ctx context.Context, namespace string, vz *pixiev1alpha1.Vizier, yamlMap map[string]string) error {
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
	err = retryDeploy(r.Clientset, r.RestConfig, namespace, resources, false)
	if err != nil {
		return err
	}

	if !vz.Spec.UseEtcdOperator {
		return nil
	}

	log.Info("Deploying etcd")
	resources, err = k8s.GetResourcesFromYAML(strings.NewReader(yamlMap["etcd"]))
	if err != nil {
		return err
	}
	for _, r := range resources {
		err = updateResourceConfiguration(r, vz)
		if err != nil {
			return err
		}
	}
	err = retryDeploy(r.Clientset, r.RestConfig, namespace, resources, false)
	if err != nil {
		return err
	}

	return nil
}

// deployVizierCore deploys the core pods and services for running vizier.
func (r *VizierReconciler) deployVizierCore(ctx context.Context, namespace string, vz *pixiev1alpha1.Vizier, yamlMap map[string]string, allowUpdate bool) error {
	log.Info("Deploying Vizier")

	vzYaml := "vizier_persistent"
	if vz.Spec.UseEtcdOperator {
		vzYaml = "vizier_etcd"
	}

	resources, err := k8s.GetResourcesFromYAML(strings.NewReader(yamlMap[vzYaml]))
	if err != nil {
		return err
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

func updateResourceConfiguration(resource *k8s.Resource, vz *pixiev1alpha1.Vizier) error {
	// Add custom labels and annotations to the k8s resource.
	addKeyValueMapToResource("labels", vz.Spec.Pod.Labels, resource.Object.Object)
	addKeyValueMapToResource("annotations", vz.Spec.Pod.Annotations, resource.Object.Object)
	updateResourceRequirements(vz.Spec.Pod.Resources, resource.Object.Object)
	return nil
}

func generateVizierYAMLs(version string, ns string, vz *pixiev1alpha1.Vizier, conn *grpc.ClientConn) (map[string]string, error) {
	var templatedYAMLs []*yamlsutils.YAMLFile
	var err error

	templatedYAMLs, err = artifacts.FetchVizierTemplates(conn, "", version)
	if err != nil {
		return nil, err
	}

	// Fill in template values.
	cloudAddr := vz.Spec.CloudAddr
	updateCloudAddr := vz.Spec.CloudAddr
	if vz.Spec.DevCloudNamespace != "" {
		cloudAddr = fmt.Sprintf("vzconn-service.%s.svc.cluster.local:51600", vz.Spec.DevCloudNamespace)
		updateCloudAddr = fmt.Sprintf("api-service.%s.svc.cluster.local:51200", vz.Spec.DevCloudNamespace)
	}
	// We should eventually clean up the templating code, since our Helm charts and extracted YAMLs will now just
	// be simple CRDs.
	tmplValues := &vizieryamls.VizierTmplValues{
		DeployKey:         vz.Spec.DeployKey,
		UseEtcdOperator:   vz.Spec.UseEtcdOperator,
		PEMMemoryLimit:    vz.Spec.PemMemoryLimit,
		Namespace:         ns,
		CloudAddr:         cloudAddr,
		CloudUpdateAddr:   updateCloudAddr,
		ClusterName:       vz.Spec.ClusterName,
		DisableAutoUpdate: vz.Spec.DisableAutoUpdate,
	}

	yamls, err := yamlsutils.ExecuteTemplatedYAMLs(templatedYAMLs, vizieryamls.VizierTmplValuesToArgs(tmplValues))
	if err != nil {
		return nil, err
	}

	// Map from the YAML name to the YAML contents.
	yamlMap := make(map[string]string)
	for _, y := range yamls {
		yamlMap[y.Name] = y.YAML
	}

	return yamlMap, nil
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
			if _, ok := s.Data["cluster-id"]; ok {
				clusterID = true
			}
		}
	}

	// TODO: Wait for the Vizier instance to actually be healthy. This may be more involved, requiring
	// the operator to read the generated jwt-signing-key and TLS certs without actually having them
	// mounted.
	return nil
}

// SetupWithManager sets up the reconciler.
func (r *VizierReconciler) SetupWithManager(mgr ctrl.Manager) error {
	return ctrl.NewControllerManagedBy(mgr).
		For(&pixiev1alpha1.Vizier{}).
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
