// Code generated by MockGen. DO NOT EDIT.
// Source: service.pb.go

// Package mock_vzmgrpb is a generated GoMock package.
package mock_vzmgrpb

import (
	context "context"
	reflect "reflect"

	types "github.com/gogo/protobuf/types"
	gomock "github.com/golang/mock/gomock"
	grpc "google.golang.org/grpc"
	uuidpb "px.dev/pixie/src/api/proto/uuidpb"
	vzmgrpb "px.dev/pixie/src/cloud/vzmgr/vzmgrpb"
	cvmsgspb "px.dev/pixie/src/shared/cvmsgspb"
)

// MockVZMgrServiceClient is a mock of VZMgrServiceClient interface.
type MockVZMgrServiceClient struct {
	ctrl     *gomock.Controller
	recorder *MockVZMgrServiceClientMockRecorder
}

// MockVZMgrServiceClientMockRecorder is the mock recorder for MockVZMgrServiceClient.
type MockVZMgrServiceClientMockRecorder struct {
	mock *MockVZMgrServiceClient
}

// NewMockVZMgrServiceClient creates a new mock instance.
func NewMockVZMgrServiceClient(ctrl *gomock.Controller) *MockVZMgrServiceClient {
	mock := &MockVZMgrServiceClient{ctrl: ctrl}
	mock.recorder = &MockVZMgrServiceClientMockRecorder{mock}
	return mock
}

// EXPECT returns an object that allows the caller to indicate expected use.
func (m *MockVZMgrServiceClient) EXPECT() *MockVZMgrServiceClientMockRecorder {
	return m.recorder
}

// CreateVizierCluster mocks base method.
func (m *MockVZMgrServiceClient) CreateVizierCluster(ctx context.Context, in *vzmgrpb.CreateVizierClusterRequest, opts ...grpc.CallOption) (*uuidpb.UUID, error) {
	m.ctrl.T.Helper()
	varargs := []interface{}{ctx, in}
	for _, a := range opts {
		varargs = append(varargs, a)
	}
	ret := m.ctrl.Call(m, "CreateVizierCluster", varargs...)
	ret0, _ := ret[0].(*uuidpb.UUID)
	ret1, _ := ret[1].(error)
	return ret0, ret1
}

// CreateVizierCluster indicates an expected call of CreateVizierCluster.
func (mr *MockVZMgrServiceClientMockRecorder) CreateVizierCluster(ctx, in interface{}, opts ...interface{}) *gomock.Call {
	mr.mock.ctrl.T.Helper()
	varargs := append([]interface{}{ctx, in}, opts...)
	return mr.mock.ctrl.RecordCallWithMethodType(mr.mock, "CreateVizierCluster", reflect.TypeOf((*MockVZMgrServiceClient)(nil).CreateVizierCluster), varargs...)
}

// GetOrgFromVizier mocks base method.
func (m *MockVZMgrServiceClient) GetOrgFromVizier(ctx context.Context, in *uuidpb.UUID, opts ...grpc.CallOption) (*vzmgrpb.GetOrgFromVizierResponse, error) {
	m.ctrl.T.Helper()
	varargs := []interface{}{ctx, in}
	for _, a := range opts {
		varargs = append(varargs, a)
	}
	ret := m.ctrl.Call(m, "GetOrgFromVizier", varargs...)
	ret0, _ := ret[0].(*vzmgrpb.GetOrgFromVizierResponse)
	ret1, _ := ret[1].(error)
	return ret0, ret1
}

// GetOrgFromVizier indicates an expected call of GetOrgFromVizier.
func (mr *MockVZMgrServiceClientMockRecorder) GetOrgFromVizier(ctx, in interface{}, opts ...interface{}) *gomock.Call {
	mr.mock.ctrl.T.Helper()
	varargs := append([]interface{}{ctx, in}, opts...)
	return mr.mock.ctrl.RecordCallWithMethodType(mr.mock, "GetOrgFromVizier", reflect.TypeOf((*MockVZMgrServiceClient)(nil).GetOrgFromVizier), varargs...)
}

// GetVizierConnectionInfo mocks base method.
func (m *MockVZMgrServiceClient) GetVizierConnectionInfo(ctx context.Context, in *uuidpb.UUID, opts ...grpc.CallOption) (*cvmsgspb.VizierConnectionInfo, error) {
	m.ctrl.T.Helper()
	varargs := []interface{}{ctx, in}
	for _, a := range opts {
		varargs = append(varargs, a)
	}
	ret := m.ctrl.Call(m, "GetVizierConnectionInfo", varargs...)
	ret0, _ := ret[0].(*cvmsgspb.VizierConnectionInfo)
	ret1, _ := ret[1].(error)
	return ret0, ret1
}

// GetVizierConnectionInfo indicates an expected call of GetVizierConnectionInfo.
func (mr *MockVZMgrServiceClientMockRecorder) GetVizierConnectionInfo(ctx, in interface{}, opts ...interface{}) *gomock.Call {
	mr.mock.ctrl.T.Helper()
	varargs := append([]interface{}{ctx, in}, opts...)
	return mr.mock.ctrl.RecordCallWithMethodType(mr.mock, "GetVizierConnectionInfo", reflect.TypeOf((*MockVZMgrServiceClient)(nil).GetVizierConnectionInfo), varargs...)
}

// GetVizierInfo mocks base method.
func (m *MockVZMgrServiceClient) GetVizierInfo(ctx context.Context, in *uuidpb.UUID, opts ...grpc.CallOption) (*cvmsgspb.VizierInfo, error) {
	m.ctrl.T.Helper()
	varargs := []interface{}{ctx, in}
	for _, a := range opts {
		varargs = append(varargs, a)
	}
	ret := m.ctrl.Call(m, "GetVizierInfo", varargs...)
	ret0, _ := ret[0].(*cvmsgspb.VizierInfo)
	ret1, _ := ret[1].(error)
	return ret0, ret1
}

// GetVizierInfo indicates an expected call of GetVizierInfo.
func (mr *MockVZMgrServiceClientMockRecorder) GetVizierInfo(ctx, in interface{}, opts ...interface{}) *gomock.Call {
	mr.mock.ctrl.T.Helper()
	varargs := append([]interface{}{ctx, in}, opts...)
	return mr.mock.ctrl.RecordCallWithMethodType(mr.mock, "GetVizierInfo", reflect.TypeOf((*MockVZMgrServiceClient)(nil).GetVizierInfo), varargs...)
}

// GetVizierInfos mocks base method.
func (m *MockVZMgrServiceClient) GetVizierInfos(ctx context.Context, in *vzmgrpb.GetVizierInfosRequest, opts ...grpc.CallOption) (*vzmgrpb.GetVizierInfosResponse, error) {
	m.ctrl.T.Helper()
	varargs := []interface{}{ctx, in}
	for _, a := range opts {
		varargs = append(varargs, a)
	}
	ret := m.ctrl.Call(m, "GetVizierInfos", varargs...)
	ret0, _ := ret[0].(*vzmgrpb.GetVizierInfosResponse)
	ret1, _ := ret[1].(error)
	return ret0, ret1
}

// GetVizierInfos indicates an expected call of GetVizierInfos.
func (mr *MockVZMgrServiceClientMockRecorder) GetVizierInfos(ctx, in interface{}, opts ...interface{}) *gomock.Call {
	mr.mock.ctrl.T.Helper()
	varargs := append([]interface{}{ctx, in}, opts...)
	return mr.mock.ctrl.RecordCallWithMethodType(mr.mock, "GetVizierInfos", reflect.TypeOf((*MockVZMgrServiceClient)(nil).GetVizierInfos), varargs...)
}

// GetViziersByOrg mocks base method.
func (m *MockVZMgrServiceClient) GetViziersByOrg(ctx context.Context, in *uuidpb.UUID, opts ...grpc.CallOption) (*vzmgrpb.GetViziersByOrgResponse, error) {
	m.ctrl.T.Helper()
	varargs := []interface{}{ctx, in}
	for _, a := range opts {
		varargs = append(varargs, a)
	}
	ret := m.ctrl.Call(m, "GetViziersByOrg", varargs...)
	ret0, _ := ret[0].(*vzmgrpb.GetViziersByOrgResponse)
	ret1, _ := ret[1].(error)
	return ret0, ret1
}

// GetViziersByOrg indicates an expected call of GetViziersByOrg.
func (mr *MockVZMgrServiceClientMockRecorder) GetViziersByOrg(ctx, in interface{}, opts ...interface{}) *gomock.Call {
	mr.mock.ctrl.T.Helper()
	varargs := append([]interface{}{ctx, in}, opts...)
	return mr.mock.ctrl.RecordCallWithMethodType(mr.mock, "GetViziersByOrg", reflect.TypeOf((*MockVZMgrServiceClient)(nil).GetViziersByOrg), varargs...)
}

// GetViziersByShard mocks base method.
func (m *MockVZMgrServiceClient) GetViziersByShard(ctx context.Context, in *vzmgrpb.GetViziersByShardRequest, opts ...grpc.CallOption) (*vzmgrpb.GetViziersByShardResponse, error) {
	m.ctrl.T.Helper()
	varargs := []interface{}{ctx, in}
	for _, a := range opts {
		varargs = append(varargs, a)
	}
	ret := m.ctrl.Call(m, "GetViziersByShard", varargs...)
	ret0, _ := ret[0].(*vzmgrpb.GetViziersByShardResponse)
	ret1, _ := ret[1].(error)
	return ret0, ret1
}

// GetViziersByShard indicates an expected call of GetViziersByShard.
func (mr *MockVZMgrServiceClientMockRecorder) GetViziersByShard(ctx, in interface{}, opts ...interface{}) *gomock.Call {
	mr.mock.ctrl.T.Helper()
	varargs := append([]interface{}{ctx, in}, opts...)
	return mr.mock.ctrl.RecordCallWithMethodType(mr.mock, "GetViziersByShard", reflect.TypeOf((*MockVZMgrServiceClient)(nil).GetViziersByShard), varargs...)
}

// UpdateOrInstallVizier mocks base method.
func (m *MockVZMgrServiceClient) UpdateOrInstallVizier(ctx context.Context, in *cvmsgspb.UpdateOrInstallVizierRequest, opts ...grpc.CallOption) (*cvmsgspb.UpdateOrInstallVizierResponse, error) {
	m.ctrl.T.Helper()
	varargs := []interface{}{ctx, in}
	for _, a := range opts {
		varargs = append(varargs, a)
	}
	ret := m.ctrl.Call(m, "UpdateOrInstallVizier", varargs...)
	ret0, _ := ret[0].(*cvmsgspb.UpdateOrInstallVizierResponse)
	ret1, _ := ret[1].(error)
	return ret0, ret1
}

// UpdateOrInstallVizier indicates an expected call of UpdateOrInstallVizier.
func (mr *MockVZMgrServiceClientMockRecorder) UpdateOrInstallVizier(ctx, in interface{}, opts ...interface{}) *gomock.Call {
	mr.mock.ctrl.T.Helper()
	varargs := append([]interface{}{ctx, in}, opts...)
	return mr.mock.ctrl.RecordCallWithMethodType(mr.mock, "UpdateOrInstallVizier", reflect.TypeOf((*MockVZMgrServiceClient)(nil).UpdateOrInstallVizier), varargs...)
}

// UpdateVizierConfig mocks base method.
func (m *MockVZMgrServiceClient) UpdateVizierConfig(ctx context.Context, in *cvmsgspb.UpdateVizierConfigRequest, opts ...grpc.CallOption) (*cvmsgspb.UpdateVizierConfigResponse, error) {
	m.ctrl.T.Helper()
	varargs := []interface{}{ctx, in}
	for _, a := range opts {
		varargs = append(varargs, a)
	}
	ret := m.ctrl.Call(m, "UpdateVizierConfig", varargs...)
	ret0, _ := ret[0].(*cvmsgspb.UpdateVizierConfigResponse)
	ret1, _ := ret[1].(error)
	return ret0, ret1
}

// UpdateVizierConfig indicates an expected call of UpdateVizierConfig.
func (mr *MockVZMgrServiceClientMockRecorder) UpdateVizierConfig(ctx, in interface{}, opts ...interface{}) *gomock.Call {
	mr.mock.ctrl.T.Helper()
	varargs := append([]interface{}{ctx, in}, opts...)
	return mr.mock.ctrl.RecordCallWithMethodType(mr.mock, "UpdateVizierConfig", reflect.TypeOf((*MockVZMgrServiceClient)(nil).UpdateVizierConfig), varargs...)
}

// VizierConnected mocks base method.
func (m *MockVZMgrServiceClient) VizierConnected(ctx context.Context, in *cvmsgspb.RegisterVizierRequest, opts ...grpc.CallOption) (*cvmsgspb.RegisterVizierAck, error) {
	m.ctrl.T.Helper()
	varargs := []interface{}{ctx, in}
	for _, a := range opts {
		varargs = append(varargs, a)
	}
	ret := m.ctrl.Call(m, "VizierConnected", varargs...)
	ret0, _ := ret[0].(*cvmsgspb.RegisterVizierAck)
	ret1, _ := ret[1].(error)
	return ret0, ret1
}

// VizierConnected indicates an expected call of VizierConnected.
func (mr *MockVZMgrServiceClientMockRecorder) VizierConnected(ctx, in interface{}, opts ...interface{}) *gomock.Call {
	mr.mock.ctrl.T.Helper()
	varargs := append([]interface{}{ctx, in}, opts...)
	return mr.mock.ctrl.RecordCallWithMethodType(mr.mock, "VizierConnected", reflect.TypeOf((*MockVZMgrServiceClient)(nil).VizierConnected), varargs...)
}

// MockVZMgrServiceServer is a mock of VZMgrServiceServer interface.
type MockVZMgrServiceServer struct {
	ctrl     *gomock.Controller
	recorder *MockVZMgrServiceServerMockRecorder
}

// MockVZMgrServiceServerMockRecorder is the mock recorder for MockVZMgrServiceServer.
type MockVZMgrServiceServerMockRecorder struct {
	mock *MockVZMgrServiceServer
}

// NewMockVZMgrServiceServer creates a new mock instance.
func NewMockVZMgrServiceServer(ctrl *gomock.Controller) *MockVZMgrServiceServer {
	mock := &MockVZMgrServiceServer{ctrl: ctrl}
	mock.recorder = &MockVZMgrServiceServerMockRecorder{mock}
	return mock
}

// EXPECT returns an object that allows the caller to indicate expected use.
func (m *MockVZMgrServiceServer) EXPECT() *MockVZMgrServiceServerMockRecorder {
	return m.recorder
}

// CreateVizierCluster mocks base method.
func (m *MockVZMgrServiceServer) CreateVizierCluster(arg0 context.Context, arg1 *vzmgrpb.CreateVizierClusterRequest) (*uuidpb.UUID, error) {
	m.ctrl.T.Helper()
	ret := m.ctrl.Call(m, "CreateVizierCluster", arg0, arg1)
	ret0, _ := ret[0].(*uuidpb.UUID)
	ret1, _ := ret[1].(error)
	return ret0, ret1
}

// CreateVizierCluster indicates an expected call of CreateVizierCluster.
func (mr *MockVZMgrServiceServerMockRecorder) CreateVizierCluster(arg0, arg1 interface{}) *gomock.Call {
	mr.mock.ctrl.T.Helper()
	return mr.mock.ctrl.RecordCallWithMethodType(mr.mock, "CreateVizierCluster", reflect.TypeOf((*MockVZMgrServiceServer)(nil).CreateVizierCluster), arg0, arg1)
}

// GetOrgFromVizier mocks base method.
func (m *MockVZMgrServiceServer) GetOrgFromVizier(arg0 context.Context, arg1 *uuidpb.UUID) (*vzmgrpb.GetOrgFromVizierResponse, error) {
	m.ctrl.T.Helper()
	ret := m.ctrl.Call(m, "GetOrgFromVizier", arg0, arg1)
	ret0, _ := ret[0].(*vzmgrpb.GetOrgFromVizierResponse)
	ret1, _ := ret[1].(error)
	return ret0, ret1
}

// GetOrgFromVizier indicates an expected call of GetOrgFromVizier.
func (mr *MockVZMgrServiceServerMockRecorder) GetOrgFromVizier(arg0, arg1 interface{}) *gomock.Call {
	mr.mock.ctrl.T.Helper()
	return mr.mock.ctrl.RecordCallWithMethodType(mr.mock, "GetOrgFromVizier", reflect.TypeOf((*MockVZMgrServiceServer)(nil).GetOrgFromVizier), arg0, arg1)
}

// GetVizierConnectionInfo mocks base method.
func (m *MockVZMgrServiceServer) GetVizierConnectionInfo(arg0 context.Context, arg1 *uuidpb.UUID) (*cvmsgspb.VizierConnectionInfo, error) {
	m.ctrl.T.Helper()
	ret := m.ctrl.Call(m, "GetVizierConnectionInfo", arg0, arg1)
	ret0, _ := ret[0].(*cvmsgspb.VizierConnectionInfo)
	ret1, _ := ret[1].(error)
	return ret0, ret1
}

// GetVizierConnectionInfo indicates an expected call of GetVizierConnectionInfo.
func (mr *MockVZMgrServiceServerMockRecorder) GetVizierConnectionInfo(arg0, arg1 interface{}) *gomock.Call {
	mr.mock.ctrl.T.Helper()
	return mr.mock.ctrl.RecordCallWithMethodType(mr.mock, "GetVizierConnectionInfo", reflect.TypeOf((*MockVZMgrServiceServer)(nil).GetVizierConnectionInfo), arg0, arg1)
}

// GetVizierInfo mocks base method.
func (m *MockVZMgrServiceServer) GetVizierInfo(arg0 context.Context, arg1 *uuidpb.UUID) (*cvmsgspb.VizierInfo, error) {
	m.ctrl.T.Helper()
	ret := m.ctrl.Call(m, "GetVizierInfo", arg0, arg1)
	ret0, _ := ret[0].(*cvmsgspb.VizierInfo)
	ret1, _ := ret[1].(error)
	return ret0, ret1
}

// GetVizierInfo indicates an expected call of GetVizierInfo.
func (mr *MockVZMgrServiceServerMockRecorder) GetVizierInfo(arg0, arg1 interface{}) *gomock.Call {
	mr.mock.ctrl.T.Helper()
	return mr.mock.ctrl.RecordCallWithMethodType(mr.mock, "GetVizierInfo", reflect.TypeOf((*MockVZMgrServiceServer)(nil).GetVizierInfo), arg0, arg1)
}

// GetVizierInfos mocks base method.
func (m *MockVZMgrServiceServer) GetVizierInfos(arg0 context.Context, arg1 *vzmgrpb.GetVizierInfosRequest) (*vzmgrpb.GetVizierInfosResponse, error) {
	m.ctrl.T.Helper()
	ret := m.ctrl.Call(m, "GetVizierInfos", arg0, arg1)
	ret0, _ := ret[0].(*vzmgrpb.GetVizierInfosResponse)
	ret1, _ := ret[1].(error)
	return ret0, ret1
}

// GetVizierInfos indicates an expected call of GetVizierInfos.
func (mr *MockVZMgrServiceServerMockRecorder) GetVizierInfos(arg0, arg1 interface{}) *gomock.Call {
	mr.mock.ctrl.T.Helper()
	return mr.mock.ctrl.RecordCallWithMethodType(mr.mock, "GetVizierInfos", reflect.TypeOf((*MockVZMgrServiceServer)(nil).GetVizierInfos), arg0, arg1)
}

// GetViziersByOrg mocks base method.
func (m *MockVZMgrServiceServer) GetViziersByOrg(arg0 context.Context, arg1 *uuidpb.UUID) (*vzmgrpb.GetViziersByOrgResponse, error) {
	m.ctrl.T.Helper()
	ret := m.ctrl.Call(m, "GetViziersByOrg", arg0, arg1)
	ret0, _ := ret[0].(*vzmgrpb.GetViziersByOrgResponse)
	ret1, _ := ret[1].(error)
	return ret0, ret1
}

// GetViziersByOrg indicates an expected call of GetViziersByOrg.
func (mr *MockVZMgrServiceServerMockRecorder) GetViziersByOrg(arg0, arg1 interface{}) *gomock.Call {
	mr.mock.ctrl.T.Helper()
	return mr.mock.ctrl.RecordCallWithMethodType(mr.mock, "GetViziersByOrg", reflect.TypeOf((*MockVZMgrServiceServer)(nil).GetViziersByOrg), arg0, arg1)
}

// GetViziersByShard mocks base method.
func (m *MockVZMgrServiceServer) GetViziersByShard(arg0 context.Context, arg1 *vzmgrpb.GetViziersByShardRequest) (*vzmgrpb.GetViziersByShardResponse, error) {
	m.ctrl.T.Helper()
	ret := m.ctrl.Call(m, "GetViziersByShard", arg0, arg1)
	ret0, _ := ret[0].(*vzmgrpb.GetViziersByShardResponse)
	ret1, _ := ret[1].(error)
	return ret0, ret1
}

// GetViziersByShard indicates an expected call of GetViziersByShard.
func (mr *MockVZMgrServiceServerMockRecorder) GetViziersByShard(arg0, arg1 interface{}) *gomock.Call {
	mr.mock.ctrl.T.Helper()
	return mr.mock.ctrl.RecordCallWithMethodType(mr.mock, "GetViziersByShard", reflect.TypeOf((*MockVZMgrServiceServer)(nil).GetViziersByShard), arg0, arg1)
}

// UpdateOrInstallVizier mocks base method.
func (m *MockVZMgrServiceServer) UpdateOrInstallVizier(arg0 context.Context, arg1 *cvmsgspb.UpdateOrInstallVizierRequest) (*cvmsgspb.UpdateOrInstallVizierResponse, error) {
	m.ctrl.T.Helper()
	ret := m.ctrl.Call(m, "UpdateOrInstallVizier", arg0, arg1)
	ret0, _ := ret[0].(*cvmsgspb.UpdateOrInstallVizierResponse)
	ret1, _ := ret[1].(error)
	return ret0, ret1
}

// UpdateOrInstallVizier indicates an expected call of UpdateOrInstallVizier.
func (mr *MockVZMgrServiceServerMockRecorder) UpdateOrInstallVizier(arg0, arg1 interface{}) *gomock.Call {
	mr.mock.ctrl.T.Helper()
	return mr.mock.ctrl.RecordCallWithMethodType(mr.mock, "UpdateOrInstallVizier", reflect.TypeOf((*MockVZMgrServiceServer)(nil).UpdateOrInstallVizier), arg0, arg1)
}

// UpdateVizierConfig mocks base method.
func (m *MockVZMgrServiceServer) UpdateVizierConfig(arg0 context.Context, arg1 *cvmsgspb.UpdateVizierConfigRequest) (*cvmsgspb.UpdateVizierConfigResponse, error) {
	m.ctrl.T.Helper()
	ret := m.ctrl.Call(m, "UpdateVizierConfig", arg0, arg1)
	ret0, _ := ret[0].(*cvmsgspb.UpdateVizierConfigResponse)
	ret1, _ := ret[1].(error)
	return ret0, ret1
}

// UpdateVizierConfig indicates an expected call of UpdateVizierConfig.
func (mr *MockVZMgrServiceServerMockRecorder) UpdateVizierConfig(arg0, arg1 interface{}) *gomock.Call {
	mr.mock.ctrl.T.Helper()
	return mr.mock.ctrl.RecordCallWithMethodType(mr.mock, "UpdateVizierConfig", reflect.TypeOf((*MockVZMgrServiceServer)(nil).UpdateVizierConfig), arg0, arg1)
}

// VizierConnected mocks base method.
func (m *MockVZMgrServiceServer) VizierConnected(arg0 context.Context, arg1 *cvmsgspb.RegisterVizierRequest) (*cvmsgspb.RegisterVizierAck, error) {
	m.ctrl.T.Helper()
	ret := m.ctrl.Call(m, "VizierConnected", arg0, arg1)
	ret0, _ := ret[0].(*cvmsgspb.RegisterVizierAck)
	ret1, _ := ret[1].(error)
	return ret0, ret1
}

// VizierConnected indicates an expected call of VizierConnected.
func (mr *MockVZMgrServiceServerMockRecorder) VizierConnected(arg0, arg1 interface{}) *gomock.Call {
	mr.mock.ctrl.T.Helper()
	return mr.mock.ctrl.RecordCallWithMethodType(mr.mock, "VizierConnected", reflect.TypeOf((*MockVZMgrServiceServer)(nil).VizierConnected), arg0, arg1)
}

// MockVZDeploymentKeyServiceClient is a mock of VZDeploymentKeyServiceClient interface.
type MockVZDeploymentKeyServiceClient struct {
	ctrl     *gomock.Controller
	recorder *MockVZDeploymentKeyServiceClientMockRecorder
}

// MockVZDeploymentKeyServiceClientMockRecorder is the mock recorder for MockVZDeploymentKeyServiceClient.
type MockVZDeploymentKeyServiceClientMockRecorder struct {
	mock *MockVZDeploymentKeyServiceClient
}

// NewMockVZDeploymentKeyServiceClient creates a new mock instance.
func NewMockVZDeploymentKeyServiceClient(ctrl *gomock.Controller) *MockVZDeploymentKeyServiceClient {
	mock := &MockVZDeploymentKeyServiceClient{ctrl: ctrl}
	mock.recorder = &MockVZDeploymentKeyServiceClientMockRecorder{mock}
	return mock
}

// EXPECT returns an object that allows the caller to indicate expected use.
func (m *MockVZDeploymentKeyServiceClient) EXPECT() *MockVZDeploymentKeyServiceClientMockRecorder {
	return m.recorder
}

// Create mocks base method.
func (m *MockVZDeploymentKeyServiceClient) Create(ctx context.Context, in *vzmgrpb.CreateDeploymentKeyRequest, opts ...grpc.CallOption) (*vzmgrpb.DeploymentKey, error) {
	m.ctrl.T.Helper()
	varargs := []interface{}{ctx, in}
	for _, a := range opts {
		varargs = append(varargs, a)
	}
	ret := m.ctrl.Call(m, "Create", varargs...)
	ret0, _ := ret[0].(*vzmgrpb.DeploymentKey)
	ret1, _ := ret[1].(error)
	return ret0, ret1
}

// Create indicates an expected call of Create.
func (mr *MockVZDeploymentKeyServiceClientMockRecorder) Create(ctx, in interface{}, opts ...interface{}) *gomock.Call {
	mr.mock.ctrl.T.Helper()
	varargs := append([]interface{}{ctx, in}, opts...)
	return mr.mock.ctrl.RecordCallWithMethodType(mr.mock, "Create", reflect.TypeOf((*MockVZDeploymentKeyServiceClient)(nil).Create), varargs...)
}

// Delete mocks base method.
func (m *MockVZDeploymentKeyServiceClient) Delete(ctx context.Context, in *vzmgrpb.DeleteDeploymentKeyRequest, opts ...grpc.CallOption) (*types.Empty, error) {
	m.ctrl.T.Helper()
	varargs := []interface{}{ctx, in}
	for _, a := range opts {
		varargs = append(varargs, a)
	}
	ret := m.ctrl.Call(m, "Delete", varargs...)
	ret0, _ := ret[0].(*types.Empty)
	ret1, _ := ret[1].(error)
	return ret0, ret1
}

// Delete indicates an expected call of Delete.
func (mr *MockVZDeploymentKeyServiceClientMockRecorder) Delete(ctx, in interface{}, opts ...interface{}) *gomock.Call {
	mr.mock.ctrl.T.Helper()
	varargs := append([]interface{}{ctx, in}, opts...)
	return mr.mock.ctrl.RecordCallWithMethodType(mr.mock, "Delete", reflect.TypeOf((*MockVZDeploymentKeyServiceClient)(nil).Delete), varargs...)
}

// Get mocks base method.
func (m *MockVZDeploymentKeyServiceClient) Get(ctx context.Context, in *vzmgrpb.GetDeploymentKeyRequest, opts ...grpc.CallOption) (*vzmgrpb.GetDeploymentKeyResponse, error) {
	m.ctrl.T.Helper()
	varargs := []interface{}{ctx, in}
	for _, a := range opts {
		varargs = append(varargs, a)
	}
	ret := m.ctrl.Call(m, "Get", varargs...)
	ret0, _ := ret[0].(*vzmgrpb.GetDeploymentKeyResponse)
	ret1, _ := ret[1].(error)
	return ret0, ret1
}

// Get indicates an expected call of Get.
func (mr *MockVZDeploymentKeyServiceClientMockRecorder) Get(ctx, in interface{}, opts ...interface{}) *gomock.Call {
	mr.mock.ctrl.T.Helper()
	varargs := append([]interface{}{ctx, in}, opts...)
	return mr.mock.ctrl.RecordCallWithMethodType(mr.mock, "Get", reflect.TypeOf((*MockVZDeploymentKeyServiceClient)(nil).Get), varargs...)
}

// List mocks base method.
func (m *MockVZDeploymentKeyServiceClient) List(ctx context.Context, in *vzmgrpb.ListDeploymentKeyRequest, opts ...grpc.CallOption) (*vzmgrpb.ListDeploymentKeyResponse, error) {
	m.ctrl.T.Helper()
	varargs := []interface{}{ctx, in}
	for _, a := range opts {
		varargs = append(varargs, a)
	}
	ret := m.ctrl.Call(m, "List", varargs...)
	ret0, _ := ret[0].(*vzmgrpb.ListDeploymentKeyResponse)
	ret1, _ := ret[1].(error)
	return ret0, ret1
}

// List indicates an expected call of List.
func (mr *MockVZDeploymentKeyServiceClientMockRecorder) List(ctx, in interface{}, opts ...interface{}) *gomock.Call {
	mr.mock.ctrl.T.Helper()
	varargs := append([]interface{}{ctx, in}, opts...)
	return mr.mock.ctrl.RecordCallWithMethodType(mr.mock, "List", reflect.TypeOf((*MockVZDeploymentKeyServiceClient)(nil).List), varargs...)
}

// LookupDeploymentKey mocks base method.
func (m *MockVZDeploymentKeyServiceClient) LookupDeploymentKey(ctx context.Context, in *vzmgrpb.LookupDeploymentKeyRequest, opts ...grpc.CallOption) (*vzmgrpb.LookupDeploymentKeyResponse, error) {
	m.ctrl.T.Helper()
	varargs := []interface{}{ctx, in}
	for _, a := range opts {
		varargs = append(varargs, a)
	}
	ret := m.ctrl.Call(m, "LookupDeploymentKey", varargs...)
	ret0, _ := ret[0].(*vzmgrpb.LookupDeploymentKeyResponse)
	ret1, _ := ret[1].(error)
	return ret0, ret1
}

// LookupDeploymentKey indicates an expected call of LookupDeploymentKey.
func (mr *MockVZDeploymentKeyServiceClientMockRecorder) LookupDeploymentKey(ctx, in interface{}, opts ...interface{}) *gomock.Call {
	mr.mock.ctrl.T.Helper()
	varargs := append([]interface{}{ctx, in}, opts...)
	return mr.mock.ctrl.RecordCallWithMethodType(mr.mock, "LookupDeploymentKey", reflect.TypeOf((*MockVZDeploymentKeyServiceClient)(nil).LookupDeploymentKey), varargs...)
}

// MockVZDeploymentKeyServiceServer is a mock of VZDeploymentKeyServiceServer interface.
type MockVZDeploymentKeyServiceServer struct {
	ctrl     *gomock.Controller
	recorder *MockVZDeploymentKeyServiceServerMockRecorder
}

// MockVZDeploymentKeyServiceServerMockRecorder is the mock recorder for MockVZDeploymentKeyServiceServer.
type MockVZDeploymentKeyServiceServerMockRecorder struct {
	mock *MockVZDeploymentKeyServiceServer
}

// NewMockVZDeploymentKeyServiceServer creates a new mock instance.
func NewMockVZDeploymentKeyServiceServer(ctrl *gomock.Controller) *MockVZDeploymentKeyServiceServer {
	mock := &MockVZDeploymentKeyServiceServer{ctrl: ctrl}
	mock.recorder = &MockVZDeploymentKeyServiceServerMockRecorder{mock}
	return mock
}

// EXPECT returns an object that allows the caller to indicate expected use.
func (m *MockVZDeploymentKeyServiceServer) EXPECT() *MockVZDeploymentKeyServiceServerMockRecorder {
	return m.recorder
}

// Create mocks base method.
func (m *MockVZDeploymentKeyServiceServer) Create(arg0 context.Context, arg1 *vzmgrpb.CreateDeploymentKeyRequest) (*vzmgrpb.DeploymentKey, error) {
	m.ctrl.T.Helper()
	ret := m.ctrl.Call(m, "Create", arg0, arg1)
	ret0, _ := ret[0].(*vzmgrpb.DeploymentKey)
	ret1, _ := ret[1].(error)
	return ret0, ret1
}

// Create indicates an expected call of Create.
func (mr *MockVZDeploymentKeyServiceServerMockRecorder) Create(arg0, arg1 interface{}) *gomock.Call {
	mr.mock.ctrl.T.Helper()
	return mr.mock.ctrl.RecordCallWithMethodType(mr.mock, "Create", reflect.TypeOf((*MockVZDeploymentKeyServiceServer)(nil).Create), arg0, arg1)
}

// Delete mocks base method.
func (m *MockVZDeploymentKeyServiceServer) Delete(arg0 context.Context, arg1 *vzmgrpb.DeleteDeploymentKeyRequest) (*types.Empty, error) {
	m.ctrl.T.Helper()
	ret := m.ctrl.Call(m, "Delete", arg0, arg1)
	ret0, _ := ret[0].(*types.Empty)
	ret1, _ := ret[1].(error)
	return ret0, ret1
}

// Delete indicates an expected call of Delete.
func (mr *MockVZDeploymentKeyServiceServerMockRecorder) Delete(arg0, arg1 interface{}) *gomock.Call {
	mr.mock.ctrl.T.Helper()
	return mr.mock.ctrl.RecordCallWithMethodType(mr.mock, "Delete", reflect.TypeOf((*MockVZDeploymentKeyServiceServer)(nil).Delete), arg0, arg1)
}

// Get mocks base method.
func (m *MockVZDeploymentKeyServiceServer) Get(arg0 context.Context, arg1 *vzmgrpb.GetDeploymentKeyRequest) (*vzmgrpb.GetDeploymentKeyResponse, error) {
	m.ctrl.T.Helper()
	ret := m.ctrl.Call(m, "Get", arg0, arg1)
	ret0, _ := ret[0].(*vzmgrpb.GetDeploymentKeyResponse)
	ret1, _ := ret[1].(error)
	return ret0, ret1
}

// Get indicates an expected call of Get.
func (mr *MockVZDeploymentKeyServiceServerMockRecorder) Get(arg0, arg1 interface{}) *gomock.Call {
	mr.mock.ctrl.T.Helper()
	return mr.mock.ctrl.RecordCallWithMethodType(mr.mock, "Get", reflect.TypeOf((*MockVZDeploymentKeyServiceServer)(nil).Get), arg0, arg1)
}

// List mocks base method.
func (m *MockVZDeploymentKeyServiceServer) List(arg0 context.Context, arg1 *vzmgrpb.ListDeploymentKeyRequest) (*vzmgrpb.ListDeploymentKeyResponse, error) {
	m.ctrl.T.Helper()
	ret := m.ctrl.Call(m, "List", arg0, arg1)
	ret0, _ := ret[0].(*vzmgrpb.ListDeploymentKeyResponse)
	ret1, _ := ret[1].(error)
	return ret0, ret1
}

// List indicates an expected call of List.
func (mr *MockVZDeploymentKeyServiceServerMockRecorder) List(arg0, arg1 interface{}) *gomock.Call {
	mr.mock.ctrl.T.Helper()
	return mr.mock.ctrl.RecordCallWithMethodType(mr.mock, "List", reflect.TypeOf((*MockVZDeploymentKeyServiceServer)(nil).List), arg0, arg1)
}

// LookupDeploymentKey mocks base method.
func (m *MockVZDeploymentKeyServiceServer) LookupDeploymentKey(arg0 context.Context, arg1 *vzmgrpb.LookupDeploymentKeyRequest) (*vzmgrpb.LookupDeploymentKeyResponse, error) {
	m.ctrl.T.Helper()
	ret := m.ctrl.Call(m, "LookupDeploymentKey", arg0, arg1)
	ret0, _ := ret[0].(*vzmgrpb.LookupDeploymentKeyResponse)
	ret1, _ := ret[1].(error)
	return ret0, ret1
}

// LookupDeploymentKey indicates an expected call of LookupDeploymentKey.
func (mr *MockVZDeploymentKeyServiceServerMockRecorder) LookupDeploymentKey(arg0, arg1 interface{}) *gomock.Call {
	mr.mock.ctrl.T.Helper()
	return mr.mock.ctrl.RecordCallWithMethodType(mr.mock, "LookupDeploymentKey", reflect.TypeOf((*MockVZDeploymentKeyServiceServer)(nil).LookupDeploymentKey), arg0, arg1)
}

// MockVZDeploymentServiceClient is a mock of VZDeploymentServiceClient interface.
type MockVZDeploymentServiceClient struct {
	ctrl     *gomock.Controller
	recorder *MockVZDeploymentServiceClientMockRecorder
}

// MockVZDeploymentServiceClientMockRecorder is the mock recorder for MockVZDeploymentServiceClient.
type MockVZDeploymentServiceClientMockRecorder struct {
	mock *MockVZDeploymentServiceClient
}

// NewMockVZDeploymentServiceClient creates a new mock instance.
func NewMockVZDeploymentServiceClient(ctrl *gomock.Controller) *MockVZDeploymentServiceClient {
	mock := &MockVZDeploymentServiceClient{ctrl: ctrl}
	mock.recorder = &MockVZDeploymentServiceClientMockRecorder{mock}
	return mock
}

// EXPECT returns an object that allows the caller to indicate expected use.
func (m *MockVZDeploymentServiceClient) EXPECT() *MockVZDeploymentServiceClientMockRecorder {
	return m.recorder
}

// RegisterVizierDeployment mocks base method.
func (m *MockVZDeploymentServiceClient) RegisterVizierDeployment(ctx context.Context, in *vzmgrpb.RegisterVizierDeploymentRequest, opts ...grpc.CallOption) (*vzmgrpb.RegisterVizierDeploymentResponse, error) {
	m.ctrl.T.Helper()
	varargs := []interface{}{ctx, in}
	for _, a := range opts {
		varargs = append(varargs, a)
	}
	ret := m.ctrl.Call(m, "RegisterVizierDeployment", varargs...)
	ret0, _ := ret[0].(*vzmgrpb.RegisterVizierDeploymentResponse)
	ret1, _ := ret[1].(error)
	return ret0, ret1
}

// RegisterVizierDeployment indicates an expected call of RegisterVizierDeployment.
func (mr *MockVZDeploymentServiceClientMockRecorder) RegisterVizierDeployment(ctx, in interface{}, opts ...interface{}) *gomock.Call {
	mr.mock.ctrl.T.Helper()
	varargs := append([]interface{}{ctx, in}, opts...)
	return mr.mock.ctrl.RecordCallWithMethodType(mr.mock, "RegisterVizierDeployment", reflect.TypeOf((*MockVZDeploymentServiceClient)(nil).RegisterVizierDeployment), varargs...)
}

// MockVZDeploymentServiceServer is a mock of VZDeploymentServiceServer interface.
type MockVZDeploymentServiceServer struct {
	ctrl     *gomock.Controller
	recorder *MockVZDeploymentServiceServerMockRecorder
}

// MockVZDeploymentServiceServerMockRecorder is the mock recorder for MockVZDeploymentServiceServer.
type MockVZDeploymentServiceServerMockRecorder struct {
	mock *MockVZDeploymentServiceServer
}

// NewMockVZDeploymentServiceServer creates a new mock instance.
func NewMockVZDeploymentServiceServer(ctrl *gomock.Controller) *MockVZDeploymentServiceServer {
	mock := &MockVZDeploymentServiceServer{ctrl: ctrl}
	mock.recorder = &MockVZDeploymentServiceServerMockRecorder{mock}
	return mock
}

// EXPECT returns an object that allows the caller to indicate expected use.
func (m *MockVZDeploymentServiceServer) EXPECT() *MockVZDeploymentServiceServerMockRecorder {
	return m.recorder
}

// RegisterVizierDeployment mocks base method.
func (m *MockVZDeploymentServiceServer) RegisterVizierDeployment(arg0 context.Context, arg1 *vzmgrpb.RegisterVizierDeploymentRequest) (*vzmgrpb.RegisterVizierDeploymentResponse, error) {
	m.ctrl.T.Helper()
	ret := m.ctrl.Call(m, "RegisterVizierDeployment", arg0, arg1)
	ret0, _ := ret[0].(*vzmgrpb.RegisterVizierDeploymentResponse)
	ret1, _ := ret[1].(error)
	return ret0, ret1
}

// RegisterVizierDeployment indicates an expected call of RegisterVizierDeployment.
func (mr *MockVZDeploymentServiceServerMockRecorder) RegisterVizierDeployment(arg0, arg1 interface{}) *gomock.Call {
	mr.mock.ctrl.T.Helper()
	return mr.mock.ctrl.RecordCallWithMethodType(mr.mock, "RegisterVizierDeployment", reflect.TypeOf((*MockVZDeploymentServiceServer)(nil).RegisterVizierDeployment), arg0, arg1)
}