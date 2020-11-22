package logmessagehandler_test

import (
	"context"
	"encoding/json"
	"fmt"
	"log"
	"os"
	"testing"

	"github.com/stretchr/testify/require"

	"github.com/gogo/protobuf/types"

	"pixielabs.ai/pixielabs/src/shared/cvmsgspb"

	logmessagehandler "pixielabs.ai/pixielabs/src/cloud/log_collector/log_message_handler"

	"github.com/nats-io/nats.go"
	"github.com/olivere/elastic/v7"
	"github.com/stretchr/testify/assert"
	"pixielabs.ai/pixielabs/src/utils/testingutils"
)

var elasticClient *elastic.Client

func publishLogMsgToNATS(t *testing.T, nc *nats.Conn, logMsg string, vizID string) {
	logMsgPb := &cvmsgspb.VLogMessage{
		Data: []byte(logMsg),
	}

	logMsgBytes, err := logMsgPb.Marshal()
	assert.Nil(t, err)

	v2cPb := &cvmsgspb.V2CMessage{
		VizierID: vizID,
		Msg: &types.Any{
			TypeUrl: "foo/pl.cvmsgspb.VLogMessage",
			Value:   logMsgBytes,
		},
	}

	v2cBytes, err := v2cPb.Marshal()
	assert.Nil(t, err)
	chanName := fmt.Sprintf("v2c.%s.%s.log", "000", vizID)
	nc.Publish(chanName, v2cBytes)
}

func assertLogMsgInElastic(ctx context.Context, t *testing.T, logMsg, vizID, indexName string, sanitizeJSONFunc func(map[string]interface{}) map[string]interface{}) {
	var j map[string]interface{}
	err := json.Unmarshal([]byte(logMsg), &j)
	require.Nil(t, err)
	j = sanitizeJSONFunc(j)

	j["cluster_id"] = vizID

	testID := j["test_id"].(string)
	query := elastic.NewMatchQuery("test_id", testID)
	searchResult, err := elasticClient.Search().
		Index(indexName).
		Query(query).
		Do(ctx)
	require.Nil(t, err)

	require.NotNilf(t, searchResult.Hits, "No search results")
	require.Equal(t, 1, len(searchResult.Hits.Hits))
	hit := searchResult.Hits.Hits[0]

	var v map[string]interface{}
	err = json.Unmarshal(hit.Source, &v)
	require.Nil(t, err)
	assert.Equal(t, j, v)
}

func TestLogMessagesSingleVizier(t *testing.T) {
	natsPort, cleanup := testingutils.StartNATS(t)
	defer cleanup()
	nc, err := nats.Connect(testingutils.GetNATSURL(natsPort))
	if err != nil {
		t.Fatal("Could not connect to NATS.")
	}
	ctx := context.Background()
	indexName := "vizier-logs"
	h := logmessagehandler.NewLogMessageHandler(ctx, nc, elasticClient, indexName)
	h.Start()

	vizID := "my-fake-cluster-id"

	for _, logMsg := range logMessages {
		publishLogMsgToNATS(t, nc, logMsg, vizID)
	}

	nc.Flush()

	h.Stop()

	for _, logMsg := range logMessages {
		assertLogMsgInElastic(ctx, t, logMsg, vizID, indexName, h.SanitizeJSONForElastic)
	}
}

func TestMain(m *testing.M) {
	es, cleanup, err := testingutils.SetupElastic()
	if err != nil {
		cleanup()
		log.Fatal(err)
	}
	elasticClient = es
	code := m.Run()
	// Can't be deferred b/c of os.Exit.
	cleanup()
	os.Exit(code)
}

var logMessages = []string{
	// NOTE: "test_id" is an added field to make it easy to check if the documents were added to elastic.
	`{
  "test_id": "1",
	"k8s_annotations": {
					"fluentbit.io/parser": "logfmt"
	},
	"k8s_container_hash": "6490081c3c65d77d059a9918f9c2d07abf3a1ec9ef14014edecf16863eabc4f0",
	"k8s_container_name": "vizier-certmgr",
	"k8s_docker_id": "eb37628d90211d02f996c51c1eaaccd9c54edd71c518a6adb0c484b60995403d",
	"k8s_host": "gke-dev-cluster-james-default-pool-fefebf2e-g2vt",
	"k8s_labels": {
					"app": "pl-monitoring",
					"app.kubernetes.io/managed-by": "skaffold-v1.3.1-12-g84eafe1cb",
					"component": "vizier",
					"name": "vizier-certmgr",
					"pod-template-hash": "64964cfc55",
					"skaffold.dev/builder": "local",
					"skaffold.dev/cleanup": "true",
					"skaffold.dev/deployer": "kustomize",
					"skaffold.dev/docker-api-version": "1.39",
					"skaffold.dev/run-id": "9dc1a00f-b1ed-4a66-8c1b-28996051ebc6",
					"skaffold.dev/tag-policy": "dateTimeTagger",
					"skaffold.dev/tail": "true"
	},
	"k8s_namespace_name": "pl",
	"k8s_pod_id": "d86b1b8a-5d86-11ea-b88e-42010a8a00a9",
	"k8s_pod_name": "vizier-certmgr-64964cfc55-gsw8w",
	"resp_size": "35",
	"resp_status": "OK",
	"resp_time": "43.612µs",
	"stream": "stdout",
	"time": "2020-03-03T19:48:51Z146319624Z"
}`,
	`{
	"test_id": "2",
	"k8s_annotations": {
					"fluentbit.io/parser": "logfmt"
	},
	"k8s_container_hash": "a9477d7e4da583c53aacae7e47a22cec2cda9ff71bd53036a9d73fac3905e928",
	"k8s_container_name": "vizier-query-broker",
	"k8s_docker_id": "ce1233182b13ea49bb5c531a2dca60c6dbfb831a9851ac219dd59e314e7f8bb9",
	"k8s_host": "gke-dev-cluster-james-default-pool-fefebf2e-g2vt",
	"k8s_labels": {
					"app": "pl-monitoring",
					"app.kubernetes.io/managed-by": "skaffold-v1.3.1-12-g84eafe1cb",
					"component": "vizier",
					"name": "vizier-query-broker",
					"pod-template-hash": "779f8dc59b",
					"skaffold.dev/builder": "local",
					"skaffold.dev/cleanup": "true",
					"skaffold.dev/deployer": "kustomize",
					"skaffold.dev/docker-api-version": "1.39",
					"skaffold.dev/run-id": "de5f4c42-0ef3-44b0-b200-09bdf3a9d8ea",
					"skaffold.dev/tag-policy": "dateTimeTagger",
					"skaffold.dev/tail": "true"
	},
	"k8s_namespace_name": "pl",
	"k8s_pod_id": "4b5d2e18-5d90-11ea-b88e-42010a8a00a9",
	"k8s_pod_name": "vizier-query-broker-779f8dc59b-m8h9h",
	"log_processed": {
					"grpc.code": "OK",
					"grpc.method": "GetAgentInfo",
					"grpc.service": "pl.vizier.services.query_broker.querybrokerpb.QueryBrokerService",
					"grpc.start_time": "2020-03-03T20:50:01Z",
					"level": "info",
					"msg": "finished unary call with code OK",
					"peer.address": "10.40.1.131:51952",
					"span.kind": "server",
					"system": "grpc",
					"time": "3.14217ms"
	},
	"stream": "stdout",
	"time": "2020-03-03T20:50:01.785901442Z"
}`,
}
