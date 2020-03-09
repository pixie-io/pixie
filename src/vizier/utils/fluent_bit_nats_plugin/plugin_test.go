package main

import (
	"encoding/hex"
	"encoding/json"
	"testing"
	"unsafe"

	"github.com/gogo/protobuf/proto"

	"github.com/stretchr/testify/assert"

	flb "github.com/fluent/fluent-bit-go/output"
	"pixielabs.ai/pixielabs/src/shared/cvmsgspb"

	"github.com/golang/mock/gomock"

	mock_main "pixielabs.ai/pixielabs/src/vizier/utils/fluent_bit_nats_plugin/mock"
)

func getJSONFromV2CMessage(V2CBytes []byte) (interface{}, error) {
	pb := &cvmsgspb.V2CMessage{}
	if err := proto.Unmarshal(V2CBytes, pb); err != nil {
		return nil, err
	}

	logPb := &cvmsgspb.VLogMessage{}
	if err := proto.Unmarshal(pb.Msg.Value, logPb); err != nil {
		return nil, err
	}
	var v interface{}
	if err := json.Unmarshal(logPb.Data, &v); err != nil {
		return nil, err
	}
	return v, nil
}

func TestPluginFlush(t *testing.T) {
	ctrl := gomock.NewController(t)
	defer ctrl.Finish()
	mockNATS := mock_main.NewMockNatsConn(ctrl)

	plugin := flb.NewPlugin()
	i := unsafe.Pointer(uintptr(0))
	flb.FLBPluginSetContext(plugin, mockNATS)

	testCases := []struct {
		Name       string
		MsgpackHex string
		OutJSON    []string
	}{
		{
			Name:       "Multiple outputs",
			MsgpackHex: msgPackSevenMsgs,
			OutJSON: []string{
				"{\"k8s_annotations\":{\"fluentbit.io/parser\":\"glog\"},\"k8s_container_hash\":\"fb7931b67ab17932be5cb2bffe641ae71557dc2ac9d426b8c55b4ef4e7403741\", \"k8s_container_name\":\"pem\",\"k8s_docker_id\":\"3aac34c50f9ec1081268e7da43bf81aa4dfa4acd26cfc1bb06682854a867402b\",\"k8s_host\":\"gke-dev-cluster-james-default-pool-fefebf2e-60l8\",\"k8s_labels\":{\"app\":\"pl-monitoring\",\"app.kubernetes.io/managed-by\":\"skaffold-v1.3.1-12-g84eafe1cb\",\"component\":\"vizier\",\"controller-revision-hash\":\"6ddf8b5f4c\",\"name\":\"vizier-pem\",\"pod-template-generation\":\"19\",\"skaffold.dev/builder\":\"local\",\"skaffold.dev/cleanup\":\"true\",\"skaffold.dev/deployer\":\"kustomize\",\"skaffold.dev/docker-api-version\":\"1.39\",\"skaffold.dev/run-id\":\"cb85501e-1c25-45da-a9d2-c6fb761baeb5\",\"skaffold.dev/tag-policy\":\"dateTimeTagger\",\"skaffold.dev/tail\":\"true\"},\"k8s_namespace_name\":\"pl\",\"k8s_pod_id\":\"671e4cb3-5e5a-11ea-b88e-42010a8a00a9\",\"k8s_pod_name\":\"vizier-pem-v69sj\",\"log_processed\":{\"file\":\"state_manager.cc\",\"level\":\"E\",\"line\":\"121\",\"msg\":\" Unhandled Update Type: 6 (ignoring)\",\"thread_id\":\"1098212\"},\"stream\":\"stderr\",\"time\":\"2020-03-04T20:55:20.434417836Z\"}",
				"{\"k8s_annotations\":{\"fluentbit.io/parser\":\"glog\"},\"k8s_container_hash\":\"fb7931b67ab17932be5cb2bffe641ae71557dc2ac9d426b8c55b4ef4e7403741\",\"k8s_container_name\":\"pem\",\"k8s_docker_id\":\"3aac34c50f9ec1081268e7da43bf81aa4dfa4acd26cfc1bb06682854a867402b\",\"k8s_host\":\"gke-dev-cluster-james-default-pool-fefebf2e-60l8\",\"k8s_labels\":{\"app\":\"pl-monitoring\",\"app.kubernetes.io/managed-by\":\"skaffold-v1.3.1-12-g84eafe1cb\",\"component\":\"vizier\",\"controller-revision-hash\":\"6ddf8b5f4c\",\"name\":\"vizier-pem\",\"pod-template-generation\":\"19\",\"skaffold.dev/builder\":\"local\",\"skaffold.dev/cleanup\":\"true\",\"skaffold.dev/deployer\":\"kustomize\",\"skaffold.dev/docker-api-version\":\"1.39\",\"skaffold.dev/run-id\":\"cb85501e-1c25-45da-a9d2-c6fb761baeb5\",\"skaffold.dev/tag-policy\":\"dateTimeTagger\",\"skaffold.dev/tail\":\"true\"},\"k8s_namespace_name\":\"pl\",\"k8s_pod_id\":\"671e4cb3-5e5a-11ea-b88e-42010a8a00a9\",\"k8s_pod_name\":\"vizier-pem-v69sj\",\"log_processed\":{\"file\":\"state_manager.cc\",\"level\":\"E\",\"line\":\"121\",\"msg\":\" Unhandled Update Type: 6 (ignoring)\",\"thread_id\":\"1098212\"},\"stream\":\"stderr\",\"time\":\"2020-03-04T20:55:20.434469988Z\"}",
				"{\"k8s_annotations\":{\"fluentbit.io/parser\":\"glog\"},\"k8s_container_hash\":\"fb7931b67ab17932be5cb2bffe641ae71557dc2ac9d426b8c55b4ef4e7403741\",\"k8s_container_name\":\"pem\",\"k8s_docker_id\":\"3aac34c50f9ec1081268e7da43bf81aa4dfa4acd26cfc1bb06682854a867402b\",\"k8s_host\":\"gke-dev-cluster-james-default-pool-fefebf2e-60l8\",\"k8s_labels\":{\"app\":\"pl-monitoring\",\"app.kubernetes.io/managed-by\":\"skaffold-v1.3.1-12-g84eafe1cb\",\"component\":\"vizier\",\"controller-revision-hash\":\"6ddf8b5f4c\",\"name\":\"vizier-pem\",\"pod-template-generation\":\"19\",\"skaffold.dev/builder\":\"local\",\"skaffold.dev/cleanup\":\"true\",\"skaffold.dev/deployer\":\"kustomize\",\"skaffold.dev/docker-api-version\":\"1.39\",\"skaffold.dev/run-id\":\"cb85501e-1c25-45da-a9d2-c6fb761baeb5\",\"skaffold.dev/tag-policy\":\"dateTimeTagger\",\"skaffold.dev/tail\":\"true\"},\"k8s_namespace_name\":\"pl\",\"k8s_pod_id\":\"671e4cb3-5e5a-11ea-b88e-42010a8a00a9\",\"k8s_pod_name\":\"vizier-pem-v69sj\",\"log_processed\":{\"file\":\"state_manager.cc\",\"level\":\"E\",\"line\":\"121\",\"msg\":\" Unhandled Update Type: 6 (ignoring)\",\"thread_id\":\"1098212\"},\"stream\":\"stderr\",\"time\":\"2020-03-04T20:55:20.434478381Z\"}",
				"{\"k8s_annotations\":{\"fluentbit.io/parser\":\"glog\"},\"k8s_container_hash\":\"fb7931b67ab17932be5cb2bffe641ae71557dc2ac9d426b8c55b4ef4e7403741\",\"k8s_container_name\":\"pem\",\"k8s_docker_id\":\"3aac34c50f9ec1081268e7da43bf81aa4dfa4acd26cfc1bb06682854a867402b\",\"k8s_host\":\"gke-dev-cluster-james-default-pool-fefebf2e-60l8\",\"k8s_labels\":{\"app\":\"pl-monitoring\",\"app.kubernetes.io/managed-by\":\"skaffold-v1.3.1-12-g84eafe1cb\",\"component\":\"vizier\",\"controller-revision-hash\":\"6ddf8b5f4c\",\"name\":\"vizier-pem\",\"pod-template-generation\":\"19\",\"skaffold.dev/builder\":\"local\",\"skaffold.dev/cleanup\":\"true\",\"skaffold.dev/deployer\":\"kustomize\",\"skaffold.dev/docker-api-version\":\"1.39\",\"skaffold.dev/run-id\":\"cb85501e-1c25-45da-a9d2-c6fb761baeb5\",\"skaffold.dev/tag-policy\":\"dateTimeTagger\",\"skaffold.dev/tail\":\"true\"},\"k8s_namespace_name\":\"pl\",\"k8s_pod_id\":\"671e4cb3-5e5a-11ea-b88e-42010a8a00a9\",\"k8s_pod_name\":\"vizier-pem-v69sj\",\"log_processed\":{\"file\":\"state_manager.cc\",\"level\":\"E\",\"line\":\"121\",\"msg\":\" Unhandled Update Type: 6 (ignoring)\",\"thread_id\":\"1098212\"},\"stream\":\"stderr\",\"time\":\"2020-03-04T20:55:20.434484162Z\"}",
				"{\"k8s_annotations\":{\"fluentbit.io/parser\":\"glog\"},\"k8s_container_hash\":\"fb7931b67ab17932be5cb2bffe641ae71557dc2ac9d426b8c55b4ef4e7403741\",\"k8s_container_name\":\"pem\",\"k8s_docker_id\":\"3aac34c50f9ec1081268e7da43bf81aa4dfa4acd26cfc1bb06682854a867402b\",\"k8s_host\":\"gke-dev-cluster-james-default-pool-fefebf2e-60l8\",\"k8s_labels\":{\"app\":\"pl-monitoring\",\"app.kubernetes.io/managed-by\":\"skaffold-v1.3.1-12-g84eafe1cb\",\"component\":\"vizier\",\"controller-revision-hash\":\"6ddf8b5f4c\",\"name\":\"vizier-pem\",\"pod-template-generation\":\"19\",\"skaffold.dev/builder\":\"local\",\"skaffold.dev/cleanup\":\"true\",\"skaffold.dev/deployer\":\"kustomize\",\"skaffold.dev/docker-api-version\":\"1.39\",\"skaffold.dev/run-id\":\"cb85501e-1c25-45da-a9d2-c6fb761baeb5\",\"skaffold.dev/tag-policy\":\"dateTimeTagger\",\"skaffold.dev/tail\":\"true\"},\"k8s_namespace_name\":\"pl\",\"k8s_pod_id\":\"671e4cb3-5e5a-11ea-b88e-42010a8a00a9\",\"k8s_pod_name\":\"vizier-pem-v69sj\",\"log_processed\":{\"file\":\"state_manager.cc\",\"level\":\"E\",\"line\":\"121\",\"msg\":\" Unhandled Update Type: 6 (ignoring)\",\"thread_id\":\"1098212\"},\"stream\":\"stderr\",\"time\":\"2020-03-04T20:55:20.434490397Z\"}",
				"{\"k8s_annotations\":{\"fluentbit.io/parser\":\"glog\"},\"k8s_container_hash\":\"fb7931b67ab17932be5cb2bffe641ae71557dc2ac9d426b8c55b4ef4e7403741\",\"k8s_container_name\":\"pem\",\"k8s_docker_id\":\"3aac34c50f9ec1081268e7da43bf81aa4dfa4acd26cfc1bb06682854a867402b\",\"k8s_host\":\"gke-dev-cluster-james-default-pool-fefebf2e-60l8\",\"k8s_labels\":{\"app\":\"pl-monitoring\",\"app.kubernetes.io/managed-by\":\"skaffold-v1.3.1-12-g84eafe1cb\",\"component\":\"vizier\",\"controller-revision-hash\":\"6ddf8b5f4c\",\"name\":\"vizier-pem\",\"pod-template-generation\":\"19\",\"skaffold.dev/builder\":\"local\",\"skaffold.dev/cleanup\":\"true\",\"skaffold.dev/deployer\":\"kustomize\",\"skaffold.dev/docker-api-version\":\"1.39\",\"skaffold.dev/run-id\":\"cb85501e-1c25-45da-a9d2-c6fb761baeb5\",\"skaffold.dev/tag-policy\":\"dateTimeTagger\",\"skaffold.dev/tail\":\"true\"},\"k8s_namespace_name\":\"pl\",\"k8s_pod_id\":\"671e4cb3-5e5a-11ea-b88e-42010a8a00a9\",\"k8s_pod_name\":\"vizier-pem-v69sj\",\"log_processed\":{\"file\":\"state_manager.cc\",\"level\":\"E\",\"line\":\"121\",\"msg\":\" Unhandled Update Type: 6 (ignoring)\",\"thread_id\":\"1098212\"},\"stream\":\"stderr\",\"time\":\"2020-03-04T20:55:20.434495739Z\"}",
				"{\"k8s_annotations\":{\"fluentbit.io/parser\":\"glog\"},\"k8s_container_hash\":\"fb7931b67ab17932be5cb2bffe641ae71557dc2ac9d426b8c55b4ef4e7403741\",\"k8s_container_name\":\"pem\",\"k8s_docker_id\":\"3aac34c50f9ec1081268e7da43bf81aa4dfa4acd26cfc1bb06682854a867402b\",\"k8s_host\":\"gke-dev-cluster-james-default-pool-fefebf2e-60l8\",\"k8s_labels\":{\"app\":\"pl-monitoring\",\"app.kubernetes.io/managed-by\":\"skaffold-v1.3.1-12-g84eafe1cb\",\"component\":\"vizier\",\"controller-revision-hash\":\"6ddf8b5f4c\",\"name\":\"vizier-pem\",\"pod-template-generation\":\"19\",\"skaffold.dev/builder\":\"local\",\"skaffold.dev/cleanup\":\"true\",\"skaffold.dev/deployer\":\"kustomize\",\"skaffold.dev/docker-api-version\":\"1.39\",\"skaffold.dev/run-id\":\"cb85501e-1c25-45da-a9d2-c6fb761baeb5\",\"skaffold.dev/tag-policy\":\"dateTimeTagger\",\"skaffold.dev/tail\":\"true\"},\"k8s_namespace_name\":\"pl\",\"k8s_pod_id\":\"671e4cb3-5e5a-11ea-b88e-42010a8a00a9\",\"k8s_pod_name\":\"vizier-pem-v69sj\",\"log_processed\":{\"file\":\"state_manager.cc\",\"level\":\"W\",\"line\":\"143\",\"msg\":\" Marking pod and its containers as dead. Likely didn't belong to this node to begin with. [pod_id=ee51a509-3e49-11ea-84ef-42010a8a001a]\",\"thread_id\":\"1098212\"},\"stream\":\"stderr\",\"time\":\"2020-03-04T20:55:20.435644294Z\"}",
			},
		},
		{
			Name:       "Time in non-date format",
			MsgpackHex: msgPackTimeMsgs,
			OutJSON: []string{
				"{\"k8s_annotations\":{\"fluentbit.io/parser\":\"logfmt\"},\"k8s_container_hash\":\"9f9e2e3219523175c527273b384be8f57ec6c6c4f29d2fae6a50678dfe899b9d\",\"k8s_container_name\":\"vizier-query-broker\",\"k8s_docker_id\":\"54522cf89ee350cf1359ee77e1c126aa0ce8f61cb5537fbdefbc5ae4b623ce22\",\"k8s_host\":\"gke-dev-cluster-james-default-pool-fefebf2e-60l8\",\"k8s_labels\":{\"app\":\"pl-monitoring\",\"app.kubernetes.io/managed-by\":\"skaffold-v1.3.1-12-g84eafe1cb\",\"component\":\"vizier\",\"name\":\"vizier-query-broker\",\"pod-template-hash\":\"5df85f9984\",\"skaffold.dev/builder\":\"local\",\"skaffold.dev/cleanup\":\"true\",\"skaffold.dev/deployer\":\"kustomize\",\"skaffold.dev/docker-api-version\":\"1.39\",\"skaffold.dev/run-id\":\"dd1d3721-761a-46ce-83f0-1cece13488b3\",\"skaffold.dev/tag-policy\":\"dateTimeTagger\",\"skaffold.dev/tail\":\"true\"},\"k8s_namespace_name\":\"pl\",\"k8s_pod_id\":\"407aadae-5e64-11ea-b88e-42010a8a00a9\",\"k8s_pod_name\":\"vizier-query-broker-5df85f9984-lfcbj\",\"log_processed\":{\"grpc.code\":\"OK\",\"grpc.method\":\"GetAgentInfo\",\"grpc.service\":\"pl.vizier.services.query_broker.querybrokerpb.QueryBrokerService\",\"grpc.start_time\":\"2020-03-04T22:07:24Z\",\"level\":\"info\",\"msg\":\"finished unary call with code OK\",\"peer.address\":\"10.40.1.84:57816\",\"span.kind\":\"server\",\"system\":\"grpc\",\"time\":\"3.798726ms\"},\"stream\":\"stdout\",\"time\":\"2020-03-04T22:07:24.873195567Z\"}",
				"{\"k8s_annotations\":{\"fluentbit.io/parser\":\"logfmt\"},\"k8s_container_hash\":\"9f9e2e3219523175c527273b384be8f57ec6c6c4f29d2fae6a50678dfe899b9d\",\"k8s_container_name\":\"vizier-query-broker\",\"k8s_docker_id\":\"54522cf89ee350cf1359ee77e1c126aa0ce8f61cb5537fbdefbc5ae4b623ce22\",\"k8s_host\":\"gke-dev-cluster-james-default-pool-fefebf2e-60l8\",\"k8s_labels\":{\"app\":\"pl-monitoring\",\"app.kubernetes.io/managed-by\":\"skaffold-v1.3.1-12-g84eafe1cb\",\"component\":\"vizier\",\"name\":\"vizier-query-broker\",\"pod-template-hash\":\"5df85f9984\",\"skaffold.dev/builder\":\"local\",\"skaffold.dev/cleanup\":\"true\",\"skaffold.dev/deployer\":\"kustomize\",\"skaffold.dev/docker-api-version\":\"1.39\",\"skaffold.dev/run-id\":\"dd1d3721-761a-46ce-83f0-1cece13488b3\",\"skaffold.dev/tag-policy\":\"dateTimeTagger\",\"skaffold.dev/tail\":\"true\"},\"k8s_namespace_name\":\"pl\",\"k8s_pod_id\":\"407aadae-5e64-11ea-b88e-42010a8a00a9\",\"k8s_pod_name\":\"vizier-query-broker-5df85f9984-lfcbj\",\"log_processed\":{\"level\":\"info\",\"msg\":\"HTTP Request\",\"req_method\":\"POST\",\"req_path\":\"/pl.vizier.services.query_broker.querybrokerpb.QueryBrokerService/GetAgentInfo\",\"resp_code\":\"200\",\"resp_size\":\"539\",\"resp_status\":\"OK\",\"resp_time\":\"4.230998ms\",\"time\":\"2020-03-04T22:07:24Z\"},\"stream\":\"stdout\",\"time\":\"2020-03-04T22:07:24.873474819Z\"}",
			},
		},
	}

	for _, tc := range testCases {
		t.Run(tc.Name, func(t *testing.T) {
			inputBytes, err := hex.DecodeString(tc.MsgpackHex)
			assert.Nil(t, err)
			data := unsafe.Pointer(&inputBytes[0])

			expects := make([]*gomock.Call, 0)
			for _, JSON := range tc.OutJSON {
				var expectV interface{}
				err := json.Unmarshal([]byte(JSON), &expectV)
				assert.Nil(t, err)
				expects = append(expects, mockNATS.EXPECT().
					Publish(V2CLogChannel, gomock.Any()).
					Do(func(_ string, V2CBytes []byte) {
						v, err := getJSONFromV2CMessage(V2CBytes)
						assert.Nil(t, err)
						assert.Equal(t, expectV, v)
					}))
			}
			gomock.InOrder(expects...)

			flbPluginFlushCtx(i, data, len(inputBytes), "this argument is ignored")
		})
	}
}
