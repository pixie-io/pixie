package controller

import (
	"context"

	"pixielabs.ai/pixielabs/src/cloud/cloudpb"
	"pixielabs.ai/pixielabs/src/cloud/vzmgr/vzmgrpb"
	uuidpb "pixielabs.ai/pixielabs/src/common/uuid/proto"
)

// Datastore is the backing store for the vzmgr service.
type Datastore interface {
	// TODO(zasgar): Figure out implementation.
}

// Server is a controller implementation of vzmgr.
type Server struct {
	d Datastore
}

// New creates a new server.
func New(datastore Datastore) *Server {
	return &Server{d: datastore}
}

// CreateVizierCluster creates a new tracked vizier cluster.
func (s *Server) CreateVizierCluster(ctx context.Context, req *vzmgrpb.CreateVizierClusterRequest) (*uuidpb.UUID, error) {
	return nil, nil
}

// GetViziersByOrg gets a list of viziers by organization.
func (s *Server) GetViziersByOrg(ctx context.Context, orgID *uuidpb.UUID) (*vzmgrpb.GetViziersByOrgResponse, error) {
	return nil, nil
}

// GetVizierInfo returns info for the specified Vizier.
func (s *Server) GetVizierInfo(ctx context.Context, req *uuidpb.UUID) (*cloudpb.VizierInfo, error) {
	return nil, nil
}

// GetVizierConnectionInfo gets a viziers connection info,
func (s *Server) GetVizierConnectionInfo(ctx context.Context, req *uuidpb.UUID) (*cloudpb.VizierConnectionInfo, error) {
	return nil, nil
}

// VizierConnected is an the request made to the mgr to handle new Vizier connections.
func (s *Server) VizierConnected(ctx context.Context, req *cloudpb.RegisterVizierRequest) (*cloudpb.RegisterVizierAck, error) {
	return nil, nil
}

// HandleVizierHeartbeat handles the heartbeat from connected viziers.
func (s *Server) HandleVizierHeartbeat(ctx context.Context, req *cloudpb.VizierHeartbeat) (*cloudpb.VizierHeartbeatAck, error) {
	return nil, nil
}
