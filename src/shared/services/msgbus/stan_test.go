package msgbus_test

import (
	"testing"

	"github.com/nats-io/stan.go"
	"github.com/spf13/viper"
	"github.com/stretchr/testify/assert"

	"pixielabs.ai/pixielabs/src/shared/services/msgbus"
	"pixielabs.ai/pixielabs/src/utils/testingutils"
)

func TestMustConnectSTAN(t *testing.T) {
	clusterID := "stan"
	clientID := "test-client"
	_, sc, cleanup := testingutils.StartStan(t, clusterID, clientID)
	defer cleanup()

	viper.Set("stan_cluster_id", clusterID)

	nc := sc.NatsConn()
	sc = msgbus.MustConnectSTAN(nc, "test-client-2")
	sub := "abc"
	data := []byte("123")
	ch := make(chan *stan.Msg)
	sc.Subscribe(sub, func(msg *stan.Msg) {
		ch <- msg
	})
	sc.Publish(sub, data)
	msg := <-ch
	assert.Equal(t, msg.Data, data)
}
