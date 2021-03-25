package tracker_test

import (
	"sync"
	"testing"

	"github.com/golang/mock/gomock"

	"pixielabs.ai/pixielabs/src/carnot/planner/distributedpb"
	"pixielabs.ai/pixielabs/src/utils/testingutils"
	"pixielabs.ai/pixielabs/src/vizier/services/metadata/metadatapb"
	mock_metadatapb "pixielabs.ai/pixielabs/src/vizier/services/metadata/metadatapb/mock"
	"pixielabs.ai/pixielabs/src/vizier/services/query_broker/tracker"
)

type fakeAgentsInfo struct {
	wg *sync.WaitGroup
	t  *testing.T
}

// ClearPendingState implementation of clear state for fake agents info.
func (a *fakeAgentsInfo) ClearPendingState() {
}

// DistributedState implementation for fake agents info.
func (a *fakeAgentsInfo) DistributedState() distributedpb.DistributedState {
	return distributedpb.DistributedState{}
}

func (a *fakeAgentsInfo) UpdateAgentsInfo(update *metadatapb.AgentUpdatesResponse) error {
	if len(update.AgentUpdates) > 0 || len(update.AgentSchemas) > 0 {
		a.wg.Done()
	}
	return nil
}

func TestAgentsInfo_GetAgentInfo(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	// This test tries out various agent state updates together and in a row to make sure
	// that they all interact with each other properly.
	testSchema := makeTestSchema(t)
	uuidpbs := makeTestAgentIDs(t)

	agents := makeTestAgents(t)
	agentDataInfos := makeTestAgentDataInfo()

	msg1 := &metadatapb.AgentUpdatesResponse{
		AgentUpdates: []*metadatapb.AgentUpdate{
			{
				AgentID: uuidpbs[0],
				Update: &metadatapb.AgentUpdate_Agent{
					Agent: agents[0],
				},
			},
			{
				AgentID: uuidpbs[0],
				Update: &metadatapb.AgentUpdate_DataInfo{
					DataInfo: agentDataInfos[0],
				},
			},
		},
		AgentSchemas: nil,
	}
	msg2 := &metadatapb.AgentUpdatesResponse{
		AgentUpdates: nil,
		AgentSchemas: testSchema,
	}

	mockClient := mock_metadatapb.NewMockMetadataServiceClient(ctrl)
	mockResp := mock_metadatapb.NewMockMetadataService_GetAgentUpdatesClient(ctrl)
	mockResp.EXPECT().Context().Return(&testingutils.MockContext{}).AnyTimes()
	mockResp.EXPECT().Recv().Return(msg1, nil)
	mockResp.EXPECT().Recv().Return(msg2, nil)
	mockResp.EXPECT().Recv().Return(&metadatapb.AgentUpdatesResponse{}, nil).AnyTimes()

	mockClient.EXPECT().
		GetAgentUpdates(gomock.Any(), gomock.Any()).
		Return(mockResp, nil)

	var wg sync.WaitGroup
	agentsInfo := &fakeAgentsInfo{&wg, t}
	agentsTracker := tracker.NewAgentsWithInfo(mockClient, "fakesigningkey", agentsInfo)
	agentsTracker.Start()
	// 2 sets of updates
	wg.Add(2)

	wg.Wait()
	agentsTracker.Stop()
}
