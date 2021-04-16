package msgbus_test

import (
	"testing"

	"github.com/nats-io/nats.go"
	"github.com/spf13/viper"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"px.dev/pixie/src/utils/testingutils"
)

func TestMustConnectNATS(t *testing.T) {
	nc, cleanup := testingutils.MustStartTestNATS(t)
	defer cleanup()

	viper.Set("nats_url", nc.ConnectedUrl())
	viper.Set("disable_ssl", true)

	sub := "sub"
	msg := []byte("test")
	ch := make(chan *nats.Msg)
	_, err := nc.ChanSubscribe(sub, ch)
	require.NoError(t, err)
	err = nc.Publish(sub, msg)
	require.NoError(t, err)
	natsMsg := <-ch
	assert.Equal(t, natsMsg.Data, msg)
}
