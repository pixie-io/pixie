package controllers_test

import (
	"testing"

	"github.com/gogo/protobuf/proto"
	"github.com/golang/mock/gomock"

	"pixielabs.ai/pixielabs/src/carnot/planner/distributedpb"
)

const (
	mutationScript = `
import pxtrace
import px

@pxtrace.probe("github.com/microservices-demo/payment.(*service).Authorise")
def probe_func():
    amount = pxtrace.ArgExpr("amount")
    return [{'amount': amount}]

pxtrace.UpsertTracepoint('http_probe3',
                    'http_return_data3',
                    probe_func,
                    px.uint128("00000003-0000-31cb-0000-00000000ab84"),
                    "5m")

px.display(px.DataFrame(table='http_return_data3'))
`
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
