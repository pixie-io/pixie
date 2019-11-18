package controllers_test

import (
	"testing"

	"github.com/stretchr/testify/assert"
	"pixielabs.ai/pixielabs/src/vizier/services/query_broker/controllers"
)

const validQueryWithFlag = `
#pl:set distributed_query=true

t1 = dataframe(table='http_events').range(start='-30s')

mapop = t1.map(fn=lambda r: {
  'time_': r.time_,
  'upid': r.upid,
  'service': r.attr.service,
  'remote_addr': r.remote_addr,
  'remote_port': r.remote_port,
  'http_resp_status': r.http_resp_status,
  'http_resp_message': r.http_resp_message,
  'http_resp_latency_ms': r.http_resp_latency_ns / 1.0E6,
  'failure': r.http_resp_status >= 400,
  
  'range_group': pl.subtract(r.time_, pl.modulo(r.time_, 1000000000)),
})
`

const validQueryWithoutFlag = `
t1 = dataframe(table='http_events').range(start='-30s')

mapop = t1.map(fn=lambda r: {
  'time_': r.time_,
  'upid': r.upid,
  'service': r.attr.service,
  'remote_addr': r.remote_addr,
  'remote_port': r.remote_port,
  'http_resp_status': r.http_resp_status,
  'http_resp_message': r.http_resp_message,
  'http_resp_latency_ms': r.http_resp_latency_ns / 1.0E6,
  'failure': r.http_resp_status >= 400,
  
  'range_group': pl.subtract(r.time_, pl.modulo(r.time_, 1000000000)),
})
`

const invalidFlag1 = `
#pl:set distributed_query=true extra
`

const invalidFlag2 = `
#pl:set distributed_query,true
`

const nonexistentFlag = `
#pl:set ABCD=efgh
`

func TestParseQueryFlags_WithFlag(t *testing.T) {
	qf, err := controllers.ParseQueryFlags(validQueryWithFlag)

	assert.Nil(t, err)
	assert.NotNil(t, qf)

	val := qf.GetBool("distributed_query")
	assert.Equal(t, true, val)

	val = qf.GetBool("invalid_key")
	assert.Equal(t, false, val)
}

func TestParseQueryFlags_NoFlag(t *testing.T) {
	qf, err := controllers.ParseQueryFlags(validQueryWithoutFlag)

	assert.Nil(t, err)
	assert.NotNil(t, qf)

	val := qf.GetBool("distributed_query")
	assert.Equal(t, false, val)

	val = qf.GetBool("invalid_key")
	assert.Equal(t, false, val)
}

func TestParseQueryFlags_InvalidFlag(t *testing.T) {
	qf, err := controllers.ParseQueryFlags(invalidFlag1)
	assert.Nil(t, qf)
	assert.NotNil(t, err)

	qf, err = controllers.ParseQueryFlags(invalidFlag2)
	assert.Nil(t, qf)
	assert.NotNil(t, err)

	qf, err = controllers.ParseQueryFlags(nonexistentFlag)
	assert.Nil(t, qf)
	assert.NotNil(t, err)
}
