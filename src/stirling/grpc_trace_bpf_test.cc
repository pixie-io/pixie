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
#include "src/stirling/testing/common.h"

constexpr std::string_view kClientPath =
    "src/stirling/http2/testing/go_grpc_client/go_grpc_client_/go_grpc_client";
constexpr std::string_view kServerPath =
    "src/stirling/http2/testing/go_grpc_server/go_grpc_server_/go_grpc_server";

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
using ::pl::stirling::testing::FindRecordIdxMatchesPID;
using ::pl::stirling::testing::GRPCStub;
using ::pl::testing::proto::EqualsProto;
using ::pl::types::ColumnWrapperRecordBatch;
using ::testing::AllOf;
using ::testing::AnyOf;
using ::testing::ElementsAre;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::SizeIs;
using ::testing::StrEq;

constexpr int kHTTPTableNum = SocketTraceConnector::kHTTPTableNum;

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
  GRPCTraceGoTest() : data_table_(kHTTPTable), ctx_(std::make_unique<StandaloneContext>()) {}

  void LaunchServer(bool use_https) {
    client_path_ = pl::testing::BazelBinTestFilePath(kClientPath).string();
    server_path_ = pl::testing::BazelBinTestFilePath(kServerPath).string();

    ASSERT_OK(fs::Exists(server_path_));
    ASSERT_OK(fs::Exists(client_path_));

    std::string https_flag = use_https ? "--https=true" : "--https=false";
    ASSERT_OK(s_.Start({server_path_, https_flag}));
    LOG(INFO) << "Server PID: " << s_.child_pid();

    // Give some time for the server to start up.
    sleep(2);

    std::string port_str;
    ASSERT_OK(s_.Stdout(&port_str));
    ASSERT_TRUE(absl::SimpleAtoi(port_str, &s_port_));
    ASSERT_NE(0, s_port_);
  }

  void InitSocketTraceConnector() {
    // Force disable protobuf parsing to output the binary protobuf in record batch.
    // Also ensure test remain passing when the default changes.
    FLAGS_stirling_enable_parsing_protobufs = false;

    // TODO(yzhao): We have to install probes after starting server. Otherwise we will run into
    // failures when detaching them. This might be relevant to probes are inherited by child process
    // when fork() and execvp().
    connector_ = SocketTraceConnector::Create("socket_trace_connector");
    PL_CHECK_OK(connector_->Init());
    connector_->InitContext(ctx_.get());
  }

  void TearDown() override {
    s_.Kill();
    EXPECT_EQ(9, s_.Wait()) << "Server should have been killed.";
    EXPECT_OK(connector_->Stop());
  }

  std::string server_path_;
  std::string client_path_;

  // Create a context to pass into each TransferData() in the test, using a dummy ASID.
  static constexpr uint32_t kASID = 1;

  DataTable data_table_;
  SubProcess s_;
  int s_port_ = -1;
  std::unique_ptr<StandaloneContext> ctx_;
  std::unique_ptr<SourceConnector> connector_;
};

class GoGRPCKProbeTraceTest : public GRPCTraceGoTest {
 protected:
  void SetUp() override {
    FLAGS_stirling_enable_grpc_kprobe_tracing = true;
    FLAGS_stirling_enable_grpc_uprobe_tracing = false;
    GRPCTraceGoTest::LaunchServer(/*use_https*/ false);
    GRPCTraceGoTest::InitSocketTraceConnector();
  }
};

TEST_F(GoGRPCKProbeTraceTest, TestGolangGrpcService) {
  // TODO(yzhao): Add a --count flag to greeter client so we can test the case of multiple RPC calls
  // (multiple HTTP2 streams).
  SubProcess c;
  ASSERT_OK(c.Start(
      {client_path_, "-name=PixieLabs", "-once", absl::StrCat("-address=localhost:", s_port_)}));

  EXPECT_EQ(0, c.Wait()) << "Client should exit normally.";

  connector_->TransferData(ctx_.get(), kHTTPTableNum, &data_table_);
  std::vector<TaggedRecordBatch> tablets = data_table_.ConsumeRecords();
  ASSERT_FALSE(tablets.empty());
  types::ColumnWrapperRecordBatch record_batch = tablets[0].records;

  for (const auto& col : record_batch) {
    // Sometimes connect() returns 0, so we might have data from requester and responder.
    ASSERT_GE(col->Size(), 1);
  }
  const std::vector<size_t> target_record_indices =
      FindRecordIdxMatchesPID(record_batch, kHTTPUPIDIdx, s_.child_pid());
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
      AnyOf(HasSubstr("127.0.0.1"), HasSubstr("::1")));
  // TODO(oazizi): This expectation broke after the switch to server-side tracing.
  // Need to replace s_port_ with client port.
  // EXPECT_EQ(s_port_,
  //           record_batch[kHTTPRemotePortIdx]->Get<types::Int64Value>(target_record_idx).val);
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

    GRPCTraceGoTest::InitSocketTraceConnector();
  }
};

// TODO(oazizi/yzhao): Looks broken with clang-10.
TEST_P(GRPCTraceUprobingTest, CaptureRPCTraceRecord) {
  // Server is launched after initializing socket tracer, which verifies that uprobes
  // are dynamically attached.
  GRPCTraceGoTest::LaunchServer(GetParam());

  connector_->InitContext(ctx_.get());

  SubProcess c;
  const std::string https_flag = GetParam() ? "--https=true" : "--https=false";
  ASSERT_OK(c.Start({client_path_, https_flag, "-once", "-name=PixieLabs",
                     absl::StrCat("-address=localhost:", s_port_)}));
  LOG(INFO) << "Client PID: " << c.child_pid();
  EXPECT_EQ(0, c.Wait());

  connector_->TransferData(ctx_.get(), kHTTPTableNum, &data_table_);

  std::vector<TaggedRecordBatch> tablets = data_table_.ConsumeRecords();
  ASSERT_FALSE(tablets.empty());
  types::ColumnWrapperRecordBatch record_batch = tablets[0].records;
  const std::vector<size_t> target_record_indices =
      FindRecordIdxMatchesPID(record_batch, kHTTPUPIDIdx, s_.child_pid());
  ASSERT_GE(target_record_indices.size(), 1);

  // We should get exactly one record.
  const size_t idx = target_record_indices.front();
  const std::string scheme_text = GetParam() ? R"(":scheme":"https")" : R"(":scheme":"http")";

  md::UPID upid(record_batch[kHTTPUPIDIdx]->Get<types::UInt128Value>(idx).val);
  std::filesystem::path proc_pid_path =
      system::Config::GetInstance().proc_path() / std::to_string(s_.child_pid());
  ASSERT_OK_AND_ASSIGN(int64_t pid_start_time, system::GetPIDStartTimeTicks(proc_pid_path));
  md::UPID expected_upid(/* asid */ 0, s_.child_pid(), pid_start_time);
  EXPECT_EQ(upid, expected_upid);

  EXPECT_THAT(
      std::string(record_batch[kHTTPReqHeadersIdx]->Get<types::StringValue>(idx)),
      AllOf(HasSubstr(absl::Substitute(R"(":authority":"localhost:$0")", s_port_)),
            HasSubstr(R"(":method":"POST")"), HasSubstr(scheme_text),
            HasSubstr(absl::StrCat(R"(":scheme":)", GetParam() ? R"("https")" : R"("http")")),
            HasSubstr(R"("content-type":"application/grpc")"), HasSubstr(R"("grpc-timeout")"),
            HasSubstr(R"("te":"trailers","user-agent")")));
  EXPECT_THAT(
      std::string(record_batch[kHTTPRespHeadersIdx]->Get<types::StringValue>(idx)),
      AllOf(HasSubstr(R"(":status":"200")"), HasSubstr(R"("content-type":"application/grpc")"),
            HasSubstr(R"("grpc-message":"")"), HasSubstr(R"("grpc-status":"0"})")));
  EXPECT_THAT(std::string(record_batch[kHTTPRemoteAddrIdx]->Get<types::StringValue>(idx)),
              AnyOf(HasSubstr("127.0.0.1"), HasSubstr("::1")));
  EXPECT_EQ(2, record_batch[kHTTPMajorVersionIdx]->Get<types::Int64Value>(idx).val);
  EXPECT_EQ(0, record_batch[kHTTPMinorVersionIdx]->Get<types::Int64Value>(idx).val);
  EXPECT_EQ(static_cast<uint64_t>(HTTPContentType::kGRPC),
            record_batch[kHTTPContentTypeIdx]->Get<types::Int64Value>(idx).val);
  EXPECT_THAT(GetHelloReply(record_batch, idx),
              EqualsProto(R"proto(message: "Hello PixieLabs")proto"));
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

    ctx_ = std::make_unique<StandaloneContext>();
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
  std::unique_ptr<StandaloneContext> ctx_;
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

  std::vector<TaggedRecordBatch> tablets = data_table_->ConsumeRecords();
  ASSERT_FALSE(tablets.empty());
  types::ColumnWrapperRecordBatch record_batch = tablets[0].records;
  std::vector<size_t> indices = FindRecordIdxMatchesPID(record_batch, kHTTPUPIDIdx, getpid());
  ASSERT_THAT(indices, SizeIs(1));
  // Was parsed as an Empty message, all fields shown as unknown fields.
  EXPECT_EQ(std::string("1: \"Hello pixielabs!\""),
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

  std::vector<TaggedRecordBatch> tablets = data_table_->ConsumeRecords();
  ASSERT_FALSE(tablets.empty());
  types::ColumnWrapperRecordBatch record_batch = tablets[0].records;
  std::vector<size_t> indices = FindRecordIdxMatchesPID(record_batch, kHTTPUPIDIdx, getpid());
  EXPECT_THAT(indices, SizeIs(12));

  for (size_t idx : indices) {
    EXPECT_THAT(std::string(record_batch[kHTTPRemoteAddrIdx]->Get<types::StringValue>(idx)),
                AnyOf(HasSubstr("127.0.0.1"), HasSubstr("::1")));
    // TODO(oazizi): After switch to server-side tracing, this expectation is wrong,
    // but no easy way to get the port from client_channel_, so disabled this for now.
    // EXPECT_EQ(runner_.port(), record_batch[kHTTPRemotePortIdx]->Get<types::Int64Value>(idx).val);
    EXPECT_EQ(2, record_batch[kHTTPMajorVersionIdx]->Get<types::Int64Value>(idx).val);
    EXPECT_EQ(static_cast<uint64_t>(HTTPContentType::kGRPC),
              record_batch[kHTTPContentTypeIdx]->Get<types::Int64Value>(idx).val);
    EXPECT_THAT(GetHelloReply(record_batch, idx),
                AnyOf(EqualsProto(R"proto(message: "Hello pixielabs!")proto"),
                      EqualsProto(R"proto(message: "Hi pixielabs!")proto")));
  }
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

  std::vector<TaggedRecordBatch> tablets = data_table_->ConsumeRecords();
  ASSERT_FALSE(tablets.empty());
  types::ColumnWrapperRecordBatch record_batch = tablets[0].records;
  std::vector<size_t> indices = FindRecordIdxMatchesPID(record_batch, kHTTPUPIDIdx, getpid());
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

  std::vector<TaggedRecordBatch> tablets = data_table_->ConsumeRecords();
  ASSERT_FALSE(tablets.empty());
  types::ColumnWrapperRecordBatch record_batch = tablets[0].records;
  std::vector<size_t> indices = FindRecordIdxMatchesPID(record_batch, kHTTPUPIDIdx, getpid());
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

  std::vector<TaggedRecordBatch> tablets = data_table_->ConsumeRecords();
  ASSERT_FALSE(tablets.empty());
  types::ColumnWrapperRecordBatch record_batch = tablets[0].records;
  std::vector<size_t> indices = FindRecordIdxMatchesPID(record_batch, kHTTPUPIDIdx, getpid());
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
  std::vector<TaggedRecordBatch> tablets = data_table_->ConsumeRecords();
  ASSERT_FALSE(tablets.empty());
  types::ColumnWrapperRecordBatch record_batch = tablets[0].records;
  std::vector<size_t> indices = FindRecordIdxMatchesPID(record_batch, kHTTPUPIDIdx, getpid());
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
