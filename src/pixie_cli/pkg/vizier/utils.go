package vizier

import (
	"errors"
	"fmt"
	"os"

	"github.com/gofrs/uuid"
	"k8s.io/client-go/rest"

	"pixielabs.ai/pixielabs/src/cloud/cloudapipb"
	cliUtils "pixielabs.ai/pixielabs/src/pixie_cli/pkg/utils"
	"pixielabs.ai/pixielabs/src/utils"
	"pixielabs.ai/pixielabs/src/utils/shared/k8s"
)

// MustConnectDefaultVizier vizier will connect to default vizier based on parameters.
func MustConnectDefaultVizier(cloudAddr string, allClusters bool, clusterID uuid.UUID) []*Connector {
	c, err := ConnectDefaultVizier(cloudAddr, allClusters, clusterID)
	if err != nil {
		cliUtils.WithError(err).Error("Failed to connect to vizier")
		os.Exit(1)
	}
	return c
}

// GetVizierList gets a list of all viziers.
func GetVizierList(cloudAddr string) ([]*cloudapipb.ClusterInfo, error) {
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

func createVizierConnection(cloudAddr string, vzInfo *cloudapipb.ClusterInfo) (*Connector, error) {
	l, err := NewLister(cloudAddr)
	if err != nil {
		return nil, err
	}

	passthrough := false
	if vzInfo.Config != nil {
		passthrough = vzInfo.Config.PassthroughEnabled
	}

	u := utils.UUIDFromProtoOrNil(vzInfo.ID)

	var vzConn *ConnectionInfo
	if !passthrough {
		vzConn, err = l.GetVizierConnection(u)
		if err != nil {
			return nil, err
		}
	}

	v, err := NewConnector(cloudAddr, vzInfo, vzConn)
	if err != nil {
		return nil, err
	}
	return v, nil
}

// ConnectDefaultVizier connects to the default vizier based on parameters.
func ConnectDefaultVizier(cloudAddr string, allClusters bool, clusterID uuid.UUID) ([]*Connector, error) {
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
		c, err := ConnectionToVizierByID(cloudAddr, clusterID)
		if err != nil {
			return nil, err
		}
		conns = append(conns, c)
		return conns, nil
	}
	return nil, fmt.Errorf("ConnectDefaultVizier expects either allClusters or clusterID to be set")
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
		if vz.Status == cloudapipb.CS_HEALTHY {
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
			cliUtils.WithError(err).Error("The current cluster in the kubeconfig not found within this org.")
			clusterID = uuid.Nil
		}
	}
	if clusterID == uuid.Nil {
		return uuid.Nil, fmt.Errorf("No current Vizier exists in kubeconfig")
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
		} else if clusterInfo.Status != cloudapipb.CS_HEALTHY {
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

// ConnectionToVizierByID connects to the vizier on specified ID.
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
func GetVizierInfo(cloudAddr string, clusterID uuid.UUID) (*cloudapipb.ClusterInfo, error) {
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

// ConnectToAllViziers connets to all available viziers.
func ConnectToAllViziers(cloudAddr string) ([]*Connector, error) {
	vzInfos, err := GetVizierList(cloudAddr)
	if err != nil {
		return nil, err
	}

	var conns []*Connector
	for _, vzInfo := range vzInfos {
		if vzInfo.Status != cloudapipb.CS_HEALTHY {
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
	s := k8s.GetSecret(clientset, "pl", "pl-cluster-secrets")
	if s == nil {
		return uuid.Nil
	}
	cID, ok := s.Data["cluster-id"]
	if !ok {
		return uuid.Nil
	}
	return uuid.FromStringOrNil(string(cID))
}
