#include <gmock/gmock.h>
#include <grpcpp/grpcpp.h>
#include <gtest/gtest.h>
#include <sys/types.h>
#include <unistd.h>

extern "C" {
#include <nghttp2/nghttp2_frame.h>
}

#include <filesystem>
#include <thread>

#include "src/common/exec/subprocess.h"
#include "src/common/testing/testing.h"
#include "src/shared/metadata/metadata.h"
#include "src/stirling/data_table.h"
#include "src/stirling/http2/grpc.h"
#include "src/stirling/http2/testing/greeter_server.h"
#include "src/stirling/http2/testing/grpc_stub.h"
#include "src/stirling/http2/testing/proto/greet.grpc.pb.h"
#include "src/stirling/socket_trace_connector.h"

DEFINE_string(go_grpc_client_path, "", "The path to the go greeter client executable.");
DEFINE_string(go_grpc_server_path, "", "The path to the go greeter server executable.");

namespace pl {
namespace stirling {

using ::grpc::Channel;
using ::pl::stirling::grpc::kGRPCMessageHeaderSizeInBytes;
using ::pl::stirling::http2::testing::Greeter;
using ::pl::stirling::http2::testing::Greeter2;
using ::pl::stirling::http2::testing::Greeter2Service;
using ::pl::stirling::http2::testing::GreeterService;
using ::pl::stirling::http2::testing::HelloReply;
using ::pl::stirling::http2::testing::HelloRequest;
using ::pl::stirling::http2::testing::ServiceRunner;
using ::pl::stirling::http2::testing::StreamingGreeter;
using ::pl::stirling::http2::testing::StreamingGreeterService;
using ::pl::stirling::testing::CreateInsecureGRPCChannel;
using ::pl::stirling::testing::GRPCStub;
using ::pl::testing::proto::EqualsProto;
using ::pl::types::ColumnWrapperRecordBatch;
using ::testing::AllOf;
using ::testing::AnyOf;
using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::MatchesRegex;
using ::testing::SizeIs;
using ::testing::StrEq;

constexpr int kHTTPTableNum = SocketTraceConnector::kHTTPTableNum;

std::vector<size_t> FindRecordIdxMatchesPid(const ColumnWrapperRecordBatch& http_record, int pid) {
  std::vector<size_t> res;
  for (size_t i = 0; i < http_record[kHTTPUPIDIdx]->Size(); ++i) {
    md::UPID upid(http_record[kHTTPUPIDIdx]->Get<types::UInt128Value>(i).val);
    if (upid.pid() == static_cast<uint64_t>(pid)) {
      res.push_back(i);
    }
  }
  return res;
}

HelloReply GetHelloReply(const ColumnWrapperRecordBatch& record_batch, const size_t idx) {
  HelloReply received_reply;
  std::string msg = record_batch[kHTTPRespBodyIdx]->Get<types::StringValue>(idx);
  if (!msg.empty()) {
    received_reply.ParseFromString(msg.substr(kGRPCMessageHeaderSizeInBytes));
  }
  return received_reply;
}

HelloRequest GetHelloRequest(const ColumnWrapperRecordBatch& record_batch, const size_t idx) {
  HelloRequest received_reply;
  std::string msg = record_batch[kHTTPReqBodyIdx]->Get<types::StringValue>(idx);
  if (!msg.empty()) {
    received_reply.ParseFromString(msg.substr(kGRPCMessageHeaderSizeInBytes));
  }
  return received_reply;
}

class GRPCTraceGoTest : public ::testing::Test {
 protected:
  GRPCTraceGoTest()
      : data_table_(kHTTPTable),
        ctx_(std::make_unique<ConnectorContext>(std::make_shared<md::AgentMetadataState>(kASID))) {}

  void Init(bool use_https) {
    CHECK(!FLAGS_go_grpc_client_path.empty())
        << "--go_grpc_client_path cannot be empty. You should run this test with bazel.";
    CHECK(std::filesystem::exists(std::filesystem::path(FLAGS_go_grpc_client_path)))
        << FLAGS_go_grpc_client_path;

    CHECK(!FLAGS_go_grpc_server_path.empty())
        << "--go_grpc_server_path cannot be empty. You should run this test with bazel.";
    CHECK(std::filesystem::exists(std::filesystem::path(FLAGS_go_grpc_server_path)))
        << FLAGS_go_grpc_server_path;

    server_path_ = FLAGS_go_grpc_server_path;
    client_path_ = FLAGS_go_grpc_client_path;

    std::string https_flag = use_https ? "--https=true" : "--https=false";
    ASSERT_OK(s_.Start({server_path_, https_flag}));

    // Give some time for the server to start up.
    sleep(2);

    const std::string port_str = s_.Stdout();
    ASSERT_TRUE(absl::SimpleAtoi(port_str, &s_port_));
    ASSERT_NE(0, s_port_);

    // Force disable protobuf parsing to output the binary protobuf in record batch.
    // Also ensure test remain passing when the default changes.
    FLAGS_stirling_enable_parsing_protobufs = false;

    // TODO(yzhao): We have to install probes after starting server. Otherwise we will run into
    // failures when detaching them. This might be relevant to probes are inherited by child process
    // when fork() and execvp().
    connector_ = SocketTraceConnector::Create("socket_trace_connector");
    socket_trace_connector_ = static_cast<SocketTraceConnector*>(connector_.get());
    CHECK(socket_trace_connector_ != nullptr);
    PL_CHECK_OK(connector_->Init());
  }

  void TearDown() override {
    s_.Kill();
    EXPECT_EQ(9, s_.Wait()) << "Server should have been killed.";
  }

  std::string server_path_;
  std::string client_path_;

  // Create a context to pass into each TransferData() in the test, using a dummy ASID.
  static constexpr uint32_t kASID = 1;

  DataTable data_table_;
  SubProcess c_;
  SubProcess s_;
  int s_port_ = -1;
  std::unique_ptr<ConnectorContext> ctx_;
  std::unique_ptr<SourceConnector> connector_;
  SocketTraceConnector* socket_trace_connector_;
};

class GoGRPCKProbeTraceTest : public GRPCTraceGoTest {
 protected:
  void SetUp() override {
    FLAGS_stirling_enable_grpc_kprobe_tracing = true;
    FLAGS_stirling_enable_grpc_uprobe_tracing = false;
    GRPCTraceGoTest::Init(/*use_https*/ false);
  }
};

TEST_F(GoGRPCKProbeTraceTest, TestGolangGrpcService) {
  // TODO(yzhao): Add a --count flag to greeter client so we can test the case of multiple RPC calls
  // (multiple HTTP2 streams).
  SubProcess c;
  ASSERT_OK(c.Start(
      {client_path_, "-name=PixieLabs", "-once", absl::StrCat("-address=localhost:", s_port_)}));

  EXPECT_OK(socket_trace_connector_->TestOnlySetTargetPID(c.child_pid()));

  EXPECT_EQ(0, c.Wait()) << "Client should exit normally.";

  types::ColumnWrapperRecordBatch& record_batch = *data_table_.ActiveRecordBatch();

  connector_->TransferData(ctx_.get(), kHTTPTableNum, &data_table_);
  for (const auto& col : record_batch) {
    // Sometimes connect() returns 0, so we might have data from requester and responder.
    ASSERT_GE(col->Size(), 1);
  }
  const std::vector<size_t> target_record_indices =
      FindRecordIdxMatchesPid(record_batch, c.child_pid());
  // We should get exactly one record.
  ASSERT_THAT(target_record_indices, SizeIs(1));
  const size_t target_record_idx = target_record_indices.front();

  EXPECT_THAT(
      std::string(record_batch[kHTTPReqHeadersIdx]->Get<types::StringValue>(target_record_idx)),
      AllOf(HasSubstr(absl::Substitute(R"(":authority":"localhost:$0")", s_port_)),
            HasSubstr(R"(":method":"POST")"),
            HasSubstr(R"(":path":"/pl.stirling.http2.testing.Greeter/SayHello")"),
            HasSubstr(R"(":scheme":"http")"), HasSubstr(R"("content-type":"application/grpc")"),
            HasSubstr(R"("grpc-timeout")"), HasSubstr(R"("te":"trailers","user-agent")")));
  EXPECT_THAT(
      std::string(record_batch[kHTTPRespHeadersIdx]->Get<types::StringValue>(target_record_idx)),
      StrEq(R"({":status":"200",)"
            R"("content-type":"application/grpc",)"
            R"("grpc-message":"",)"
            R"("grpc-status":"0"})"));
  EXPECT_THAT(
      std::string(record_batch[kHTTPRemoteAddrIdx]->Get<types::StringValue>(target_record_idx)),
      HasSubstr("127.0.0.1"));
  EXPECT_EQ(s_port_,
            record_batch[kHTTPRemotePortIdx]->Get<types::Int64Value>(target_record_idx).val);
  EXPECT_EQ(2, record_batch[kHTTPMajorVersionIdx]->Get<types::Int64Value>(target_record_idx).val);
  EXPECT_EQ(static_cast<uint64_t>(HTTPContentType::kGRPC),
            record_batch[kHTTPContentTypeIdx]->Get<types::Int64Value>(target_record_idx).val);

  EXPECT_THAT(GetHelloReply(record_batch, target_record_idx),
              EqualsProto(R"proto(message: "Hello PixieLabs")proto"));
}

class GRPCTraceUprobingTest : public GRPCTraceGoTest, public ::testing::WithParamInterface<bool> {
 protected:
  void SetUp() override {
    FLAGS_stirling_enable_grpc_kprobe_tracing = false;
    FLAGS_stirling_enable_grpc_uprobe_tracing = true;
    GRPCTraceGoTest::Init(GetParam());

    const std::string https_flag = GetParam() ? "--https=true" : "--https=false";
    ASSERT_OK(c_.Start({client_path_, https_flag, absl::StrCat("-address=localhost:", s_port_)}));
  }
};

// TODO(PL-1297): Fix the bug and re-enable this test.
TEST_P(GRPCTraceUprobingTest, DISABLED_CaptureRPCTraceRecord) {
  // Give some time for the client to execute and produce data into perf buffers.
  sleep(2);
  connector_->TransferData(ctx_.get(), kHTTPTableNum, &data_table_);

  types::ColumnWrapperRecordBatch& record_batch = *data_table_.ActiveRecordBatch();
  const std::vector<size_t> target_record_indices =
      FindRecordIdxMatchesPid(record_batch, c_.child_pid());
  EXPECT_THAT(target_record_indices, Not(IsEmpty()));

  // TODO(yzhao): We should have the same check on the trace record as
  // GRPCTraceGoTest.TestGolangGrpcService.
}

INSTANTIATE_TEST_SUITE_P(SecurityModeTest, GRPCTraceUprobingTest, ::testing::Values(true, false));

class GRPCCppTest : public ::testing::Test {
 protected:
  void SetUp() override {
    SetUpSocketTraceConnector();
    SetUpGRPCServices();
  }

  void SetUpSocketTraceConnector() {
    // Force disable protobuf parsing to output the binary protobuf in record batch.
    // Also ensure test remain passing when the default changes.
    FLAGS_stirling_enable_parsing_protobufs = false;
    FLAGS_stirling_enable_grpc_kprobe_tracing = true;
    // TODO(yzhao): Stirling DFATALs if kprobe and uprobe are working simultaneously, which would
    // clobber the events. The default is already false, and Google test claims to restore flag
    // values after each test. Not sure why this is needed.
    FLAGS_stirling_enable_grpc_uprobe_tracing = false;
    FLAGS_stirling_disable_self_tracing = false;

    source_ = SocketTraceConnector::Create("bcc_grpc_trace");
    ASSERT_OK(source_->Init());

    auto* socket_trace_connector = static_cast<SocketTraceConnector*>(source_.get());
    ASSERT_NE(nullptr, socket_trace_connector);

    data_table_ = std::make_unique<DataTable>(kHTTPTable);

    // Create a context to pass into each TransferData() in the test, using a dummy ASID.
    static constexpr uint32_t kASID = 1;
    auto agent_metadata_state = std::make_shared<md::AgentMetadataState>(kASID);
    ctx_ = std::make_unique<ConnectorContext>(std::move(agent_metadata_state));
  }

  void SetUpGRPCServices() {
    runner_.RegisterService(&greeter_service_);
    runner_.RegisterService(&greeter2_service_);
    runner_.RegisterService(&streaming_greeter_service_);

    server_ = runner_.Run();

    auto* server_ptr = server_.get();
    server_thread_ = std::thread([server_ptr]() { server_ptr->Wait(); });

    client_channel_ = CreateInsecureGRPCChannel(absl::StrCat("127.0.0.1:", runner_.port()));
    greeter_stub_ = std::make_unique<GRPCStub<Greeter>>(client_channel_);
    greeter2_stub_ = std::make_unique<GRPCStub<Greeter2>>(client_channel_);
    streaming_greeter_stub_ = std::make_unique<GRPCStub<StreamingGreeter>>(client_channel_);
  }

  void TearDown() override {
    ASSERT_OK(source_->Stop());
    server_->Shutdown();
    if (server_thread_.joinable()) {
      server_thread_.join();
    }
  }

  template <typename StubType, typename RPCMethodType>
  std::vector<::grpc::Status> CallRPC(StubType* stub, RPCMethodType method,
                                      const std::vector<std::string>& names) {
    std::vector<::grpc::Status> res;
    HelloRequest req;
    HelloReply resp;
    for (const auto& n : names) {
      req.set_name(n);
      res.push_back(stub->CallRPC(method, req, &resp));
    }
    return res;
  }

  std::unique_ptr<SourceConnector> source_;
  std::unique_ptr<ConnectorContext> ctx_;
  std::unique_ptr<DataTable> data_table_;

  GreeterService greeter_service_;
  Greeter2Service greeter2_service_;
  StreamingGreeterService streaming_greeter_service_;

  ServiceRunner runner_;
  std::unique_ptr<::grpc::Server> server_;
  std::thread server_thread_;

  std::shared_ptr<Channel> client_channel_;

  std::unique_ptr<GRPCStub<Greeter>> greeter_stub_;
  std::unique_ptr<GRPCStub<Greeter2>> greeter2_stub_;
  std::unique_ptr<GRPCStub<StreamingGreeter>> streaming_greeter_stub_;
};

TEST_F(GRPCCppTest, ParseTextProtoSimpleUnaryRPCCall) {
  FLAGS_stirling_enable_parsing_protobufs = true;
  CallRPC(greeter_stub_.get(), &Greeter::Stub::SayHello, {"pixielabs"});
  source_->TransferData(ctx_.get(), kHTTPTableNum, data_table_.get());

  types::ColumnWrapperRecordBatch& record_batch = *data_table_->ActiveRecordBatch();
  std::vector<size_t> indices = FindRecordIdxMatchesPid(record_batch, getpid());
  ASSERT_THAT(indices, SizeIs(1));
  // Was parsed as an Empty message, all fields shown as unknown fields.
  EXPECT_EQ(std::string("1: \"Hello pixielabs!\"\n"),
            std::string(record_batch[kHTTPRespBodyIdx]->Get<types::StringValue>(indices[0])));
}

TEST_F(GRPCCppTest, MixedGRPCServicesOnSameGRPCChannel) {
  // TODO(yzhao): Put CallRPC() calls inside multiple threads. That would cause header parsing
  // failures, debug and fix the root cause.
  CallRPC(greeter_stub_.get(), &Greeter::Stub::SayHello, {"pixielabs", "pixielabs", "pixielabs"});
  CallRPC(greeter_stub_.get(), &Greeter::Stub::SayHelloAgain,
          {"pixielabs", "pixielabs", "pixielabs"});
  CallRPC(greeter2_stub_.get(), &Greeter2::Stub::SayHi, {"pixielabs", "pixielabs", "pixielabs"});
  CallRPC(greeter2_stub_.get(), &Greeter2::Stub::SayHiAgain,
          {"pixielabs", "pixielabs", "pixielabs"});
  source_->TransferData(ctx_.get(), kHTTPTableNum, data_table_.get());

  types::ColumnWrapperRecordBatch& record_batch = *data_table_->ActiveRecordBatch();
  std::vector<size_t> indices = FindRecordIdxMatchesPid(record_batch, getpid());
  EXPECT_THAT(indices, SizeIs(12));

  for (size_t idx : indices) {
    EXPECT_THAT(std::string(record_batch[kHTTPRemoteAddrIdx]->Get<types::StringValue>(idx)),
                HasSubstr("127.0.0.1"));
    EXPECT_EQ(runner_.port(), record_batch[kHTTPRemotePortIdx]->Get<types::Int64Value>(idx).val);
    EXPECT_EQ(2, record_batch[kHTTPMajorVersionIdx]->Get<types::Int64Value>(idx).val);
    EXPECT_EQ(static_cast<uint64_t>(HTTPContentType::kGRPC),
              record_batch[kHTTPContentTypeIdx]->Get<types::Int64Value>(idx).val);
    EXPECT_THAT(GetHelloReply(record_batch, idx),
                AnyOf(EqualsProto(R"proto(message: "Hello pixielabs!")proto"),
                      EqualsProto(R"proto(message: "Hi pixielabs!")proto")));
  }
}

// Tests to show the captured results from a timed out RPC call.
// TODO(yzhao): Sometime the tracer can still capture data, which breaks Jeninks build. Root causing
// and fix it.
TEST_F(GRPCCppTest, DISABLED_RPCTimesOut) {
  greeter_service_.set_enable_cond_wait(true);
  auto statuses = CallRPC(greeter_stub_.get(), &Greeter::Stub::SayHello, {"pixielabs"});
  ASSERT_THAT(statuses, SizeIs(1));
  EXPECT_EQ(::grpc::StatusCode::DEADLINE_EXCEEDED, statuses[0].error_code());

  source_->TransferData(ctx_.get(), kHTTPTableNum, data_table_.get());

  types::ColumnWrapperRecordBatch& record_batch = *data_table_->ActiveRecordBatch();
  std::vector<size_t> indices = FindRecordIdxMatchesPid(record_batch, getpid());
  // TODO(yzhao): ATM missing response, here because of response times out, renders requests being
  // held in buffer and not exported. Change to export requests after a certain timeout.
  EXPECT_THAT(indices, IsEmpty());

  // Wait for RPC call to timeout, and then unblock the server.
  greeter_service_.Notify();
}

template <typename ProtoType>
std::vector<ProtoType> ParseProtobufRecords(absl::string_view buf) {
  std::vector<ProtoType> res;
  while (!buf.empty()) {
    const uint32_t len = nghttp2_get_uint32(reinterpret_cast<const uint8_t*>(buf.data()) + 1);
    ProtoType reply;
    reply.ParseFromArray(buf.data() + kGRPCMessageHeaderSizeInBytes, len);
    res.push_back(std::move(reply));
    buf.remove_prefix(kGRPCMessageHeaderSizeInBytes + len);
  }
  return res;
}

// Tests that a streaming RPC call will keep a HTTP2 stream open for the entirety of the RPC call.
// Therefore if the server takes a long time to return the results, the trace record would not
// be exported until then.
// TODO(yzhao): We need some way to export streaming RPC trace record gradually.
TEST_F(GRPCCppTest, ServerStreamingRPC) {
  HelloRequest req;
  req.set_name("pixielabs");
  req.set_count(3);

  std::vector<HelloReply> replies;

  ::grpc::Status st = streaming_greeter_stub_->CallServerStreamingRPC(
      &StreamingGreeter::Stub::SayHelloServerStreaming, req, &replies);
  EXPECT_TRUE(st.ok());
  EXPECT_THAT(replies, SizeIs(3));

  source_->TransferData(ctx_.get(), kHTTPTableNum, data_table_.get());

  types::ColumnWrapperRecordBatch& record_batch = *data_table_->ActiveRecordBatch();
  std::vector<size_t> indices = FindRecordIdxMatchesPid(record_batch, getpid());
  EXPECT_THAT(indices, SizeIs(1));

  for (size_t idx : indices) {
    EXPECT_THAT(GetHelloRequest(record_batch, idx),
                EqualsProto(R"proto(name: "pixielabs" count: 3)proto"));
    EXPECT_THAT(ParseProtobufRecords<HelloReply>(
                    record_batch[kHTTPRespBodyIdx]->Get<types::StringValue>(idx)),
                ElementsAre(EqualsProto("message: 'Hello pixielabs for no. 0!'"),
                            EqualsProto("message: 'Hello pixielabs for no. 1!'"),
                            EqualsProto("message: 'Hello pixielabs for no. 2!'")));
  }
}

TEST_F(GRPCCppTest, BidirStreamingRPC) {
  HelloRequest req1;
  req1.set_name("foo");
  req1.set_count(1);

  HelloRequest req2;
  req2.set_name("bar");
  req2.set_count(1);

  std::vector<HelloReply> replies;

  ::grpc::Status st = streaming_greeter_stub_->CallBidirStreamingRPC(
      &StreamingGreeter::Stub::SayHelloBidirStreaming, {req1, req2}, &replies);
  EXPECT_TRUE(st.ok());
  EXPECT_THAT(replies, SizeIs(2));

  source_->TransferData(ctx_.get(), kHTTPTableNum, data_table_.get());

  types::ColumnWrapperRecordBatch& record_batch = *data_table_->ActiveRecordBatch();
  std::vector<size_t> indices = FindRecordIdxMatchesPid(record_batch, getpid());
  EXPECT_THAT(indices, SizeIs(1));

  for (size_t idx : indices) {
    EXPECT_THAT(ParseProtobufRecords<HelloRequest>(
                    record_batch[kHTTPReqBodyIdx]->Get<types::StringValue>(idx)),
                ElementsAre(EqualsProto(R"proto(name: "foo" count: 1)proto"),
                            EqualsProto(R"proto(name: "bar" count: 1)proto")));
    EXPECT_THAT(ParseProtobufRecords<HelloReply>(
                    record_batch[kHTTPRespBodyIdx]->Get<types::StringValue>(idx)),
                ElementsAre(EqualsProto("message: 'Hello foo for no. 0!'"),
                            EqualsProto("message: 'Hello bar for no. 0!'")));
  }
}

// Do not initialize socket tracer to simulate the socket tracer missing head of start of the HTTP2
// connection.
class GRPCCppMiddleInterceptTest : public GRPCCppTest {
 protected:
  void SetUp() { SetUpGRPCServices(); }
};

TEST_F(GRPCCppMiddleInterceptTest, InterceptMiddleOfTheConnection) {
  CallRPC(greeter_stub_.get(), &Greeter::Stub::SayHello, {"pixielabs", "pixielabs", "pixielabs"});

  // Attach the probes after connection started.
  SetUpSocketTraceConnector();
  CallRPC(greeter_stub_.get(), &Greeter::Stub::SayHello, {"pixielabs", "pixielabs", "pixielabs"});
  source_->TransferData(ctx_.get(), kHTTPTableNum, data_table_.get());

  types::ColumnWrapperRecordBatch& record_batch = *data_table_->ActiveRecordBatch();
  std::vector<size_t> indices = FindRecordIdxMatchesPid(record_batch, getpid());
  EXPECT_THAT(indices, SizeIs(3));
  for (size_t idx : indices) {
    // Header parsing would fail, because missing the head of start.
    // TODO(yzhao): We should device some meaningful mechanism for capturing headers, if inflation
    // failed.
    EXPECT_THAT(GetHelloReply(record_batch, idx),
                AnyOf(EqualsProto(R"proto(message: "Hello pixielabs!")proto"),
                      EqualsProto(R"proto(message: "Hi pixielabs!")proto")));
  }
}

class GRPCCppCallingNonRegisteredServiceTest : public GRPCCppTest {
 protected:
  void SetUp() {
    SetUpSocketTraceConnector();

    runner_.RegisterService(&greeter2_service_);
    server_ = runner_.Run();

    auto* server_ptr = server_.get();
    server_thread_ = std::thread([server_ptr]() { server_ptr->Wait(); });

    client_channel_ = CreateInsecureGRPCChannel(absl::StrCat("127.0.0.1:", runner_.port()));
    greeter_stub_ = std::make_unique<GRPCStub<Greeter>>(client_channel_);
  }
};

// Tests to show what is captured when calling a remote endpoint that does not implement the
// requested method.
TEST_F(GRPCCppCallingNonRegisteredServiceTest, ResultsAreAsExpected) {
  CallRPC(greeter_stub_.get(), &Greeter::Stub::SayHello, {"pixielabs", "pixielabs", "pixielabs"});
  source_->TransferData(ctx_.get(), kHTTPTableNum, data_table_.get());
  types::ColumnWrapperRecordBatch& record_batch = *data_table_->ActiveRecordBatch();
  std::vector<size_t> indices = FindRecordIdxMatchesPid(record_batch, getpid());
  EXPECT_THAT(indices, SizeIs(3));
  for (size_t idx : indices) {
    EXPECT_THAT(std::string(record_batch[kHTTPRespHeadersIdx]->Get<types::StringValue>(idx)),
                StrEq(R"({":status":"200",)"
                      R"("content-type":"application/grpc",)"
                      R"("grpc-status":"12"})"));
    EXPECT_THAT(GetHelloRequest(record_batch, idx), EqualsProto(R"proto(name: "pixielabs")proto"));
    EXPECT_THAT(record_batch[kHTTPRespBodyIdx]->Get<types::StringValue>(idx), IsEmpty());
  }
}

}  // namespace stirling
}  // namespace pl
