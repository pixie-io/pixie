package controllers_test

import (
	"testing"

	"github.com/gogo/protobuf/proto"
	"github.com/golang/mock/gomock"

	"pixielabs.ai/pixielabs/src/carnot/planner/distributedpb"
)

func TestMutationExecutor_Execute(t *testing.T) {
	// Set up mocks.
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()

	plannerStatePB := new(distributedpb.LogicalPlannerState)
	if err := proto.UnmarshalText(singleAgentDistributedState, plannerStatePB); err != nil {
		t.Fatal("Cannot Unmarshal protobuf.")
	}

	t.Skip("These tests are incomplete and need to be done")
}
