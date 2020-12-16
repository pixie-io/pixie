package vzutils_test

import (
	"errors"
	"sync"
	"testing"

	"github.com/golang/mock/gomock"
	"github.com/nats-io/nats.go"
	uuid "github.com/satori/go.uuid"
	"github.com/stretchr/testify/assert"

	"pixielabs.ai/pixielabs/src/cloud/shared/messages"
	messagespb "pixielabs.ai/pixielabs/src/cloud/shared/messagespb"
	"pixielabs.ai/pixielabs/src/cloud/shared/vzutils"
	"pixielabs.ai/pixielabs/src/cloud/vzmgr/vzmgrpb"
	mock_vzmgrpb "pixielabs.ai/pixielabs/src/cloud/vzmgr/vzmgrpb/mock"
	"pixielabs.ai/pixielabs/src/utils"
	"pixielabs.ai/pixielabs/src/utils/testingutils"
)

func TestVzWatcher(t *testing.T) {
	tests := []struct {
		name        string
		expectError bool
	}{
		{
			name:        "no error",
			expectError: false,
		},
		{
			name:        "error",
			expectError: true,
		},
	}

	for _, test := range tests {
		t.Run(test.name, func(t *testing.T) {
			ctrl := gomock.NewController(t)

			mockVZMgr := mock_vzmgrpb.NewMockVZMgrServiceClient(ctrl)

			existingVzID := uuid.NewV4()
			existingOrgID := uuid.NewV4()
			existingK8sUID := "testUID"
			existingResourceVersion := "1234"

			mockVZMgr.
				EXPECT().
				GetViziersByShard(gomock.Any(), &vzmgrpb.GetViziersByShardRequest{
					FromShardID: "00",
					ToShardID:   "bb",
				}).
				Return(&vzmgrpb.GetViziersByShardResponse{
					Viziers: []*vzmgrpb.GetViziersByShardResponse_VizierInfo{
						&vzmgrpb.GetViziersByShardResponse_VizierInfo{
							VizierID:        utils.ProtoFromUUID(existingVzID),
							OrgID:           utils.ProtoFromUUID(existingOrgID),
							ResourceVersion: existingResourceVersion,
							K8sUID:          existingK8sUID,
						},
					},
				}, nil)

			natsPort, natsCleanup := testingutils.StartNATS(t)
			nc, err := nats.Connect(testingutils.GetNATSURL(natsPort))
			if err != nil {
				t.Fatal(err)
			}
			defer natsCleanup()

			w, err := vzutils.NewWatcher(nc, mockVZMgr, "00", "bb")
			assert.Nil(t, err)

			var wg sync.WaitGroup
			wg.Add(2)

			if test.expectError {
				wg.Add(1)
			}

			defer wg.Wait()

			newVzID := uuid.NewV4()
			newOrgID := uuid.NewV4()
			newK8sUID := "testUID"
			newResourceVersion := "1234"

			w.RegisterErrorHandler(func(id uuid.UUID, orgID uuid.UUID, uid string, rv string, err error) {
				defer wg.Done()
				assert.Equal(t, existingVzID, id)
				assert.Equal(t, existingOrgID, orgID)
				assert.Equal(t, existingK8sUID, uid)
				assert.Equal(t, existingResourceVersion, rv)
			})

			w.RegisterVizierHandler(func(id uuid.UUID, orgID uuid.UUID, uid string, rv string) error {
				defer wg.Done()

				if id == existingVzID {
					assert.Equal(t, existingOrgID, orgID)
					assert.Equal(t, existingK8sUID, uid)
					assert.Equal(t, existingResourceVersion, rv)
					if test.expectError {
						return errors.New("Some error")
					}
				} else if id == newVzID {
					assert.Equal(t, newOrgID, orgID)
					assert.Equal(t, newK8sUID, uid)
					assert.Equal(t, newResourceVersion, rv)
				} else {
					t.Fatal("Called Vizier handler with unexpected vizier")
				}
				return nil
			})

			msg := &messagespb.VizierConnected{
				VizierID:        utils.ProtoFromUUID(newVzID),
				OrgID:           utils.ProtoFromUUID(newOrgID),
				ResourceVersion: newResourceVersion,
				K8sUID:          newK8sUID,
			}
			b, err := msg.Marshal()
			assert.Nil(t, err)
			nc.Publish(messages.VizierConnectedChannel, b)
		})
	}
}
