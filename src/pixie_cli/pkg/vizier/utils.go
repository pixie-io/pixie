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

package vizier

import (
	"context"
	"errors"
	"fmt"
	"os"
	"strings"

	"github.com/gofrs/uuid"
	metav1 "k8s.io/apimachinery/pkg/apis/meta/v1"
	"k8s.io/client-go/kubernetes"
	"k8s.io/client-go/rest"

	"px.dev/pixie/src/api/proto/cloudpb"
	cliUtils "px.dev/pixie/src/pixie_cli/pkg/utils"
	"px.dev/pixie/src/utils"
	"px.dev/pixie/src/utils/shared/k8s"
)

// FindVizierNamespace looks for the namespace that the vizier is running in for the current context.
func FindVizierNamespace(clientset *kubernetes.Clientset) (string, error) {
	vzPods, err := clientset.CoreV1().Pods("").List(context.Background(), metav1.ListOptions{
		LabelSelector: "component=vizier",
	})
	if err != nil {
		return "", err
	}

	if len(vzPods.Items) == 0 {
		return "", nil
	}

	return vzPods.Items[0].Namespace, nil
}

// FindOperatorNamespace finds the namespace running the vizier-operator.
func FindOperatorNamespace(clientset *kubernetes.Clientset) (string, error) {
	labelSelector := metav1.FormatLabelSelector(&metav1.LabelSelector{
		MatchExpressions: []metav1.LabelSelectorRequirement{
			{
				Key:      "app",
				Operator: metav1.LabelSelectorOpIn,
				Values:   []string{"pixie-operator"},
			},
		},
	})

	vzPods, err := clientset.CoreV1().Pods("").List(context.Background(), metav1.ListOptions{
		LabelSelector: labelSelector,
	})
	if err != nil {
		return "", err
	}

	if len(vzPods.Items) == 0 {
		return "", nil
	}

	return vzPods.Items[0].Namespace, nil
}

// MustFindVizierNamespace exits the current program if a Vizier namespace can't be found.
func MustFindVizierNamespace() string {
	kubeConfig := k8s.GetConfig()
	clientset := k8s.GetClientset(kubeConfig)
	vzNs, err := FindVizierNamespace(clientset)
	if err != nil {
		fmt.Printf("Failed to get Vizier namespace: %s\n", err.Error())
		os.Exit(1)
	}
	if vzNs == "" {
		fmt.Println("Cannot find running Vizier instance")
		os.Exit(0)
	}
	return vzNs
}

// MustConnectVizier will connect to Pixie cloud or directly to a vizier service.
func MustConnectVizier(cloudAddr string, allClusters bool, clusterID uuid.UUID, directVzAddr string, directKey string) []*Connector {
	if directVzAddr == "" {
		return MustConnectHealthyDefaultVizier(cloudAddr, allClusters, clusterID)
	}

	conn, err := NewConnector(cloudAddr, nil, directVzAddr, directKey)
	if err != nil {
		cliUtils.WithError(err).Fatal("Failed to connect to vizier")
	}
	return []*Connector{conn}
}

// MustConnectHealthyDefaultVizier vizier will connect to default vizier based on parameters.
func MustConnectHealthyDefaultVizier(cloudAddr string, allClusters bool, clusterID uuid.UUID) []*Connector {
	c, err := ConnectHealthyDefaultVizier(cloudAddr, allClusters, clusterID)
	if err != nil {
		cliUtils.WithError(err).Fatal("Failed to connect to vizier")
	}
	return c
}

// GetVizierList gets a list of all viziers.
func GetVizierList(cloudAddr string) ([]*cloudpb.ClusterInfo, error) {
	l, err := NewLister(cloudAddr)
	if err != nil {
		return nil, err
	}

	vzInfo, err := l.GetViziersInfo()
	if err != nil {
		return nil, err
	}

	if len(vzInfo) == 0 {
		return nil, errors.New("no Viziers available")
	}
	return vzInfo, nil
}

func createVizierConnection(cloudAddr string, vzInfo *cloudpb.ClusterInfo) (*Connector, error) {
	v, err := NewConnector(cloudAddr, vzInfo, "", "")
	if err != nil {
		return nil, err
	}
	return v, nil
}

// ConnectHealthyDefaultVizier connects to the healthy default vizier based on parameters.
func ConnectHealthyDefaultVizier(cloudAddr string, allClusters bool, clusterID uuid.UUID) ([]*Connector, error) {
	var conns []*Connector
	if allClusters {
		var err error
		conns, err = ConnectToAllViziers(cloudAddr)
		if err != nil {
			return nil, err
		}
		return conns, nil
	}
	if clusterID != uuid.Nil {
		c, err := ConnectionToHealthyVizierByID(cloudAddr, clusterID)
		if err != nil {
			return nil, err
		}
		conns = append(conns, c)
		return conns, nil
	}
	return nil, fmt.Errorf("ConnectHealthyDefaultVizier expects either allClusters or clusterID to be set")
}

// FirstHealthyVizier returns the cluster ID of the first healthy vizier.
func FirstHealthyVizier(cloudAddr string) (uuid.UUID, error) {
	l, err := NewLister(cloudAddr)
	if err != nil {
		return uuid.Nil, err
	}

	vzInfo, err := l.GetViziersInfo()
	if err != nil {
		return uuid.Nil, err
	}

	if len(vzInfo) == 0 {
		return uuid.Nil, errors.New("no Viziers available")
	}

	// Find the first healthy vizier by default.
	for _, vz := range vzInfo {
		if vz.Status == cloudpb.CS_HEALTHY {
			return utils.UUIDFromProtoOrNil(vz.ID), nil
		}
	}
	// If no healthy vizier was found, try to look for a degraded cluster.
	for _, vz := range vzInfo {
		if vz.Status == cloudpb.CS_DEGRADED {
			return utils.UUIDFromProtoOrNil(vz.ID), nil
		}
	}
	return uuid.Nil, errors.New("no healthy Viziers available")
}

// GetCurrentVizier tries to get the ID of the current Vizier, even if it is unhealthy.
func GetCurrentVizier(cloudAddr string) (uuid.UUID, error) {
	var clusterID uuid.UUID
	config := k8s.GetConfig()
	if config != nil {
		clusterID = GetClusterIDFromKubeConfig(config)
	}
	if clusterID != uuid.Nil {
		_, err := GetVizierInfo(cloudAddr, clusterID)
		if err != nil {
			cliUtils.WithError(err).Error("the current cluster in the kubeconfig not found within this org")
			clusterID = uuid.Nil
		}
	}
	if clusterID == uuid.Nil {
		return uuid.Nil, fmt.Errorf("the current cluster in the kubeconfig does not have Pixie installed")
	}
	return clusterID, nil
}

// GetCurrentOrFirstHealthyVizier tries to get the vizier from the current context. If unavailable, it gets the ID of
// the first healthy Vizier.
func GetCurrentOrFirstHealthyVizier(cloudAddr string) (uuid.UUID, error) {
	var clusterID uuid.UUID
	var err error
	config := k8s.GetConfig()
	if config != nil {
		clusterID = GetClusterIDFromKubeConfig(config)
	}
	if clusterID != uuid.Nil {
		clusterInfo, err := GetVizierInfo(cloudAddr, clusterID)
		if err != nil {
			cliUtils.WithError(err).Error("The current cluster in the kubeconfig not found within this org.")
			clusterID = uuid.Nil
		} else if clusterInfo.Status != cloudpb.CS_HEALTHY && clusterInfo.Status != cloudpb.CS_DEGRADED {
			cliUtils.WithError(err).Errorf("'%s'in the kubeconfig's Pixie instance is unhealthy.", clusterInfo.PrettyClusterName)
			clusterID = uuid.Nil
		}
	}
	if clusterID == uuid.Nil {
		clusterID, err = FirstHealthyVizier(cloudAddr)
		if err != nil {
			return uuid.Nil, errors.New("Could not fetch healthy vizier")
		}
		clusterInfo, err := GetVizierInfo(cloudAddr, clusterID)
		if err != nil {
			return uuid.Nil, errors.New("Could not fetch healthy vizier")
		}
		cliUtils.WithError(err).Infof("Running on '%s' instead.", clusterInfo.PrettyClusterName)
	}

	return clusterID, nil
}

// ConnectionToHealthyVizierByID connects to the input clusterID if it is healthy.
// It returns an error if the clusterID provided corresponds to a cluster that is not healthy.
func ConnectionToHealthyVizierByID(cloudAddr string, clusterID uuid.UUID) (*Connector, error) {
	clusterInfo, err := GetVizierInfo(cloudAddr, clusterID)
	if err != nil {
		return nil, errors.New("Could not fetch vizier")
	}
	if clusterInfo.Status != cloudpb.CS_HEALTHY && clusterInfo.Status != cloudpb.CS_DEGRADED {
		msg := fmt.Sprintf("Cluster %s is not healthy. Status is %s.",
			clusterID.String(), clusterInfo.Status.String())
		if clusterInfo.StatusMessage != "" {
			msg = msg + fmt.Sprintf(" Status Message: %s", clusterInfo.StatusMessage)
		}
		return nil, errors.New(msg)
	}
	if clusterInfo.Status == cloudpb.CS_DEGRADED {
		cliUtils.Infof("Data may not be complete.\nCluster '%s' is in a degraded state: %s", clusterID.String(), clusterInfo.StatusMessage)
	}
	return ConnectionToVizierByID(cloudAddr, clusterID)
}

// ConnectionToVizierByID connects to the vizier on specified ID.
// It will not check for Vizier health.
func ConnectionToVizierByID(cloudAddr string, clusterID uuid.UUID) (*Connector, error) {
	vzInfos, err := GetVizierList(cloudAddr)
	if err != nil {
		return nil, err
	}

	for _, vzInfo := range vzInfos {
		if utils.UUIDFromProtoOrNil(vzInfo.ID) == clusterID {
			return createVizierConnection(cloudAddr, vzInfo)
		}
	}

	return nil, errors.New("no such vizier")
}

// GetVizierInfo returns the info about the vizier running on the specified cluster, if it exists.
func GetVizierInfo(cloudAddr string, clusterID uuid.UUID) (*cloudpb.ClusterInfo, error) {
	vzInfos, err := GetVizierList(cloudAddr)
	if err != nil {
		return nil, err
	}

	for _, vzInfo := range vzInfos {
		if utils.UUIDFromProtoOrNil(vzInfo.ID) == clusterID {
			return vzInfo, err
		}
	}

	return nil, errors.New("no such vizier")
}

// ConnectToAllViziers connects to all available viziers.
func ConnectToAllViziers(cloudAddr string) ([]*Connector, error) {
	vzInfos, err := GetVizierList(cloudAddr)
	if err != nil {
		return nil, err
	}

	var conns []*Connector
	for _, vzInfo := range vzInfos {
		if vzInfo.Status != cloudpb.CS_HEALTHY && vzInfo.Status != cloudpb.CS_DEGRADED {
			continue
		}
		c, err := createVizierConnection(cloudAddr, vzInfo)
		if err != nil {
			return nil, err
		}
		conns = append(conns, c)
	}

	return conns, nil
}

// GetClusterIDFromKubeConfig returns the clusterID given the kubeconfig. If anything fails, then will return a nil UUID.
func GetClusterIDFromKubeConfig(config *rest.Config) uuid.UUID {
	if config == nil {
		return uuid.Nil
	}
	clientset := k8s.GetClientset(config)
	if clientset == nil {
		return uuid.Nil
	}
	vzNs, err := FindVizierNamespace(clientset)
	if err != nil || vzNs == "" {
		return uuid.Nil
	}
	s := k8s.GetSecret(clientset, vzNs, "pl-cluster-secrets")
	if s == nil {
		return uuid.Nil
	}
	cID, ok := s.Data["cluster-id"]
	if !ok {
		return uuid.Nil
	}
	return uuid.FromStringOrNil(string(cID))
}

// GetCloudAddrFromKubeConfig returns the cloud address given the kubeconfig. If anything fails, then will return an empty string.
func GetCloudAddrFromKubeConfig(config *rest.Config) string {
	if config == nil {
		return ""
	}
	clientset := k8s.GetClientset(config)
	if clientset == nil {
		return ""
	}
	vzNs, err := FindVizierNamespace(clientset)
	if err != nil || vzNs == "" {
		return ""
	}

	cm, err := clientset.CoreV1().ConfigMaps(vzNs).Get(context.Background(), "pl-cloud-config", metav1.GetOptions{})
	if err != nil {
		return ""
	}

	cloudAddr, ok := cm.Data["PL_CLOUD_ADDR"]
	if !ok {
		return ""
	}
	if strings.Contains(cloudAddr, "vzconn-service") && strings.Contains(cloudAddr, "svc.cluster.local") {
		cloudAddr = strings.Replace(cloudAddr, "vzconn-service", "cloud-proxy-service", 1)
		splitAddr := strings.Split(cloudAddr, ":")
		if len(splitAddr) < 1 {
			return cloudAddr
		}
		cloudAddr = fmt.Sprintf("%s:443", splitAddr[0])
	}

	return cloudAddr
}
