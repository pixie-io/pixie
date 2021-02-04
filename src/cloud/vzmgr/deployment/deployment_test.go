package deployment_test

import (
	"context"
	"errors"
	"testing"

	uuid "github.com/satori/go.uuid"
	"github.com/stretchr/testify/assert"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"
	"pixielabs.ai/pixielabs/src/cloud/vzmgr/deployment"
	"pixielabs.ai/pixielabs/src/cloud/vzmgr/vzerrors"
	"pixielabs.ai/pixielabs/src/cloud/vzmgr/vzmgrpb"
	"pixielabs.ai/pixielabs/src/utils"
)

var (
	testOrgID  = uuid.FromStringOrNil("223e4567-e89b-12d3-a456-426655440000")
	testUserID = uuid.FromStringOrNil("423e4567-e89b-12d3-a456-426655440000")

	testValidClusterID = uuid.FromStringOrNil("553e4567-e89b-12d3-a456-426655440000")

	testValidDeploymentKey = "883e4567-e89b-12d3-a456-426655440000"
)

type fakeDF struct{}

func (f *fakeDF) FetchOrgUserIDUsingDeploymentKey(ctx context.Context, key string) (uuid.UUID, uuid.UUID, error) {
	if key == testValidDeploymentKey {
		return testOrgID, testUserID, nil
	}
	return uuid.Nil, uuid.Nil, vzerrors.ErrDeploymentKeyNotFound
}

type fakeProvisioner struct {
}

func (f *fakeProvisioner) ProvisionOrClaimVizier(ctx context.Context, orgID uuid.UUID, userID uuid.UUID, clusterUID string, clusterName string, clusterVersion string) (uuid.UUID, error) {
	if testOrgID == orgID && testUserID == userID && clusterUID == "cluster1" && clusterName == "test" && clusterVersion == "1.1" {
		return testValidClusterID, nil
	}
	if testOrgID == orgID && testUserID == userID && clusterUID == "cluster2" {
		return uuid.Nil, vzerrors.ErrProvisionFailedVizierIsActive
	}
	return uuid.Nil, errors.New("bad request")
}

func TestService_RegisterVizierDeployment(t *testing.T) {
	svc := deployment.New(&fakeDF{}, &fakeProvisioner{})

	ctx := context.Background()
	resp, err := svc.RegisterVizierDeployment(ctx, &vzmgrpb.RegisterVizierDeploymentRequest{
		K8sClusterUID:     "cluster1",
		DeploymentKey:     testValidDeploymentKey,
		K8sClusterName:    "test",
		K8sClusterVersion: "1.1",
	})
	assert.Nil(t, err)
	assert.NotNil(t, resp)
	assert.Equal(t, testValidClusterID, utils.UUIDFromProtoOrNil(resp.VizierID))
}

func TestService_RegisterVizierDeployment_ClusterAlreadyRunning(t *testing.T) {
	svc := deployment.New(&fakeDF{}, &fakeProvisioner{})

	ctx := context.Background()
	resp, err := svc.RegisterVizierDeployment(ctx, &vzmgrpb.RegisterVizierDeploymentRequest{
		K8sClusterUID: "cluster2",
		DeploymentKey: testValidDeploymentKey,
	})
	assert.Nil(t, resp)
	assert.NotNil(t, err)
	assert.Equal(t, codes.ResourceExhausted, status.Code(err))
}

func TestService_RegisterVizierDeployment_InvalidDeployKey(t *testing.T) {
	svc := deployment.New(&fakeDF{}, &fakeProvisioner{})

	ctx := context.Background()
	resp, err := svc.RegisterVizierDeployment(ctx, &vzmgrpb.RegisterVizierDeploymentRequest{
		K8sClusterUID: "cluster2",
		DeploymentKey: "a bad key",
	})
	assert.Nil(t, resp)
	assert.NotNil(t, err)
	assert.Equal(t, codes.Unauthenticated, status.Code(err))
}
