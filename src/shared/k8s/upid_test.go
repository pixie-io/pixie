package k8s_test

import (
	"testing"

	"github.com/stretchr/testify/assert"

	"pixielabs.ai/pixielabs/src/shared/k8s"
	"pixielabs.ai/pixielabs/src/shared/types"
)

func TestUPID(t *testing.T) {
	upid := &types.UInt128{
		Low:  uint64(89101),
		High: uint64(528280977975),
	}

	assert.Equal(t, uint32(123), k8s.ASIDFromUPID(upid))
	assert.Equal(t, uint32(567), k8s.PIDFromUPID(upid))
	assert.Equal(t, uint64(89101), k8s.StartTSFromUPID(upid))
	assert.Equal(t, "123:567:89101", k8s.StringFromUPID(upid))
}
