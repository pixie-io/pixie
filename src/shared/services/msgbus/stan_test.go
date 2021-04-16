package msgbus_test

import (
	"testing"

	"github.com/nats-io/stan.go"
	"github.com/spf13/viper"
	"github.com/stretchr/testify/assert"
	"github.com/stretchr/testify/require"

	"px.dev/pixie/src/shared/services/msgbus"
	"px.dev/pixie/src/utils/testingutils"
)

func TestMustConnectSTAN(t *testing.T) {
	clusterID := "stan"
	clientID := "test-client"
	_, sc, cleanup := testingutils.MustStartTestStan(t, clusterID, clientID)
	defer cleanup()

	viper.Set("stan_cluster_id", clusterID)

	nc := sc.NatsConn()
	sc = msgbus.MustConnectSTAN(nc, "test-client-2")
	sub := "abc"
	data := []byte("123")
	ch := make(chan *stan.Msg)
	_, err := sc.Subscribe(sub, func(msg *stan.Msg) {
		ch <- msg
	})
	require.NoError(t, err)
	err = sc.Publish(sub, data)
	require.NoError(t, err)
	msg := <-ch
	assert.Equal(t, msg.Data, data)
}
