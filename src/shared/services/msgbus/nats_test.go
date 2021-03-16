package msgbus_test

import (
	"testing"

	"github.com/nats-io/nats.go"
	"github.com/spf13/viper"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"pixielabs.ai/pixielabs/src/shared/services/msgbus"
	"pixielabs.ai/pixielabs/src/utils/testingutils"
)

func TestMustConnectNATS(t *testing.T) {
	port, cleanup := testingutils.StartNATS(t)
	defer cleanup()

	viper.Set("nats_url", testingutils.GetNATSURL(port))
	viper.Set("disable_ssl", true)
	nc := msgbus.MustConnectNATS()
	sub := "sub"
	msg := []byte("test")
	ch := make(chan *nats.Msg)
	_, err := nc.ChanSubscribe(sub, ch)
	require.Nil(t, err)
	nc.Publish(sub, msg)
	natsMsg := <-ch
	assert.Equal(t, natsMsg.Data, msg)
}
