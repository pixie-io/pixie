/*
 * Copyright 2018- The Pixie Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "experimental/protobufs/aliasing.h"

#include <gmock/gmock.h>
#include <google/protobuf/compiler/importer.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/text_format.h>
#include <gtest/gtest.h>

#include "absl/strings/substitute.h"
#include "experimental/protobufs/proto/test.pb.h"
#include "src/common/base/utils.h"
#include "src/common/grpcutils/service_descriptor_database.h"
#include "src/common/testing/testing.h"

namespace experimental {
namespace protobufs {
namespace {

using ::google::protobuf::DescriptorProto;
using ::google::protobuf::FileDescriptorProto;
using ::google::protobuf::FileDescriptorSet;
using ::google::protobuf::Message;
using ::google::protobuf::TextFormat;
using ::google::protobuf::UnknownField;
using ::google::protobuf::UnknownFieldSet;
using ::google::protobuf::compiler::DiskSourceTree;
using ::google::protobuf::compiler::SourceTreeDescriptorDatabase;
using ::px::grpc::ServiceDescriptorDatabase;
using ::px::testing::proto::EqualsProto;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::SizeIs;

// References:
// * https://developers.google.com/protocol-buffers/docs/encoding

// Notes:
//
// * UnknownField::Type::TYPE_GROUP is deprecated in proto2, and is removed from proto3.
// So TYPE_GROUP can only appear if parsing a proto2 formatted message. In our case, we should not
// need to worry about TYPE_GROUP, as proto2 is not recommended for gRPC (the bottom of
// https://grpc.io/docs/guides/).

std::unique_ptr<ServiceDescriptorDatabase> CreateServiceDescDB() {
  FileDescriptorSet fd_set;
  Foo1::descriptor()->file()->CopyTo(fd_set.add_file());
  return std::make_unique<ServiceDescriptorDatabase>(fd_set);
}

class ProtobufsAliasingTest : public ::testing::Test {
 protected:
  ProtobufsAliasingTest() : db_(CreateServiceDescDB()) {}

  std::unique_ptr<ServiceDescriptorDatabase> db_;
};

TEST_F(ProtobufsAliasingTest, EmptyMessagesSerializeToEmptyString) {
  Foo1 foo1;
  EXPECT_THAT(foo1.SerializeAsString(), IsEmpty());
  Foo2 foo2;
  EXPECT_THAT(foo2.SerializeAsString(), IsEmpty());
}

// Tests that messages with fields of the same name and type serialize to the same string.
TEST_F(ProtobufsAliasingTest, MessagesWithSameFieldsSerializeToSameString) {
  Foo1 foo1;
  foo1.set_i32(100);
  Foo2 foo2;
  foo2.set_i32(100);
  EXPECT_EQ(foo1.SerializeAsString(), foo2.SerializeAsString());
}

std::vector<UnknownField::Type> GetUnknownFieldsType(const Message& message) {
  const google::protobuf::Reflection* refl = message.GetReflection();
  const UnknownFieldSet& unknown_fields = refl->GetUnknownFields(message);
  std::vector<UnknownField::Type> unknown_fields_type;
  for (int i = 0; i < unknown_fields.field_count(); ++i) {
    unknown_fields_type.push_back(unknown_fields.field(i).type());
  }
  return unknown_fields_type;
}

TEST_F(ProtobufsAliasingTest, MessagesWithFieldsOfDifferentNumbers) {
  Foo3 foo3;
  foo3.set_f32(100.0);
  foo3.set_str("test");
  foo3.mutable_foo2()->set_i32(100);
  foo3.mutable_bar3()->set_i32(100);
  foo3.add_i32s(100);

  auto foo1 = db_->GetMessage("experimental.Foo1");
  ASSERT_NE(nullptr, foo1);

  // A well-formed serialized protobuf string can be parsed successfully by any other message.
  // Unrecognized fields are put into UnknownFieldSet.
  EXPECT_TRUE(foo1->ParseFromString(foo3.SerializeAsString()));
  // The field number 1 is fixed32 in Foo3, but int32 in Foo1.
  EXPECT_THAT(GetUnknownFieldsType(*foo1), ElementsAre(UnknownField::Type::TYPE_FIXED32,
                                                       UnknownField::Type::TYPE_LENGTH_DELIMITED,
                                                       UnknownField::Type::TYPE_LENGTH_DELIMITED,
                                                       UnknownField::Type::TYPE_LENGTH_DELIMITED,
                                                       UnknownField::Type::TYPE_LENGTH_DELIMITED));
}

TEST_F(ProtobufsAliasingTest, MessagesWithFieldWithCompatibleTypes) {
  MessageWithCompatibleFields1 msg1;
  msg1.set_i32(100);
  msg1.set_ui32(100);
  msg1.set_si32(100);
  msg1.set_tf(true);
  msg1.set_i32a(100);

  MessageWithCompatibleFields2 msg2;
  msg2.set_i64(100);
  msg2.set_ui64(100);
  msg2.set_si64(100);
  msg2.set_tf(MessageWithCompatibleFields2::True);
  msg2.set_ui32(100);

  EXPECT_EQ(msg1.SerializeAsString(), msg2.SerializeAsString());
}

TEST_F(ProtobufsAliasingTest, MessagesWithLengthDelimitedFields) {
  MessageWithLengthDelimitedFields1 msg1;
  msg1.set_bs("test");
  msg1.set_str("test");
  msg1.mutable_msg()->set_i32(100);
  msg1.mutable_foo1()->set_i32(100);
  msg1.set_str2("test");

  auto msg2 = db_->GetMessage("experimental.MessageWithLengthDelimitedFields2");
  EXPECT_TRUE(msg2->ParseFromString(msg1.SerializeAsString()));
  EXPECT_THAT(GetUnknownFieldsType(*msg2), IsEmpty());
  // Repeated primitive field can be packed.
  EXPECT_THAT(*msg2, EqualsProto(absl::Substitute(R"proto(
                                                      bs: "test"
                                                      str: "test"
                                                      msg_str: "$0"
                                                      msg_bytes: "$0"
                                                      ri32: 116
                                                      ri32: 101
                                                      ri32: 115
                                                      ri32: 116)proto",
                                                  "\010d")));
}

TEST_F(ProtobufsAliasingTest, StringAndPackedRepeated) {
  {
    RepeatedInt32 ri32;
    ri32.add_ri32s(100);
    ri32.add_ri32s(101);
    ri32.add_ri32s(102);

    auto str = db_->GetMessage("experimental.String");
    ASSERT_NE(nullptr, str);
    EXPECT_TRUE(str->ParseFromString(ri32.SerializeAsString()));
    EXPECT_THAT(GetUnknownFieldsType(*str), IsEmpty());
    EXPECT_THAT(*str, EqualsProto("str: 'def'"));
  }
  {
    String str;
    str.set_str("0123456789abc");

    auto ri32 = db_->GetMessage("experimental.RepeatedInt32");
    ASSERT_NE(nullptr, ri32);
    EXPECT_TRUE(ri32->ParseFromString(str.SerializeAsString()));
    EXPECT_THAT(GetUnknownFieldsType(*ri32), IsEmpty());
    EXPECT_THAT(*ri32, EqualsProto(R"proto(
        ri32s: 48
        ri32s: 49
        ri32s: 50
        ri32s: 51
        ri32s: 52
        ri32s: 53
        ri32s: 54
        ri32s: 55
        ri32s: 56
        ri32s: 57
        ri32s: 97
        ri32s: 98
        ri32s: 99
    )proto"));
  }
}

TEST_F(ProtobufsAliasingTest, FieldsWithDifferentWireTypes) {
  DifferentWireTypes1 msg1;
  msg1.set_i321(100);
  msg1.set_i322(100);
  msg1.set_i323(100);

  msg1.set_f321(100);
  msg1.set_f322(100);
  msg1.set_f323(100);

  msg1.set_f641(100);
  msg1.set_f642(100);
  msg1.set_f643(100);

  msg1.set_str1("abc");
  msg1.set_str2("abc");
  msg1.set_str3("abc");

  auto msg2 = db_->GetMessage("experimental.DifferentWireTypes2");
  ASSERT_NE(nullptr, msg2);

  EXPECT_TRUE(msg2->ParseFromString(msg1.SerializeAsString()));
  EXPECT_THAT(
      GetUnknownFieldsType(*msg2),
      ElementsAre(UnknownField::Type::TYPE_VARINT, UnknownField::Type::TYPE_VARINT,
                  UnknownField::Type::TYPE_VARINT, UnknownField::Type::TYPE_FIXED32,
                  UnknownField::Type::TYPE_FIXED32, UnknownField::Type::TYPE_FIXED32,
                  UnknownField::Type::TYPE_FIXED64, UnknownField::Type::TYPE_FIXED64,
                  UnknownField::Type::TYPE_FIXED64, UnknownField::Type::TYPE_LENGTH_DELIMITED,
                  UnknownField::Type::TYPE_LENGTH_DELIMITED,
                  UnknownField::Type::TYPE_LENGTH_DELIMITED));
}

TEST_F(ProtobufsAliasingTest, VarintWireFormatFieldsToBool) {
  VarintWireFormatFields msg1;
  msg1.set_i32(100);
  msg1.set_i64(100);
  msg1.set_u32(100);
  msg1.set_u64(100);
  msg1.set_s32(100);
  msg1.set_s64(100);
  msg1.set_boo(true);
  msg1.set_enum_value(VarintWireFormatFields::EnumValue2);

  BoolFields msg2;
  EXPECT_TRUE(msg2.ParseFromString(msg1.SerializeAsString()));
  EXPECT_THAT(GetUnknownFieldsType(msg2), IsEmpty());
  EXPECT_TRUE(msg2.boo1());
  EXPECT_TRUE(msg2.boo2());
  EXPECT_TRUE(msg2.boo3());
  EXPECT_TRUE(msg2.boo4());
  EXPECT_TRUE(msg2.boo5());
  EXPECT_TRUE(msg2.boo6());
  EXPECT_TRUE(msg2.boo7());
  EXPECT_TRUE(msg2.boo8());
}

TEST_F(ProtobufsAliasingTest, Fixed32AndFixed64Fields) {
  {
    Fixed32Fields msg1;
    msg1.add_f32s(100);
    msg1.add_f32s(100);

    Fixed64Fields msg2;
    EXPECT_TRUE(msg2.ParseFromString(msg1.SerializeAsString()));
    EXPECT_THAT(GetUnknownFieldsType(msg2), IsEmpty());
  }
  {
    Fixed64Fields msg2;
    msg2.add_f64s(100);
    Fixed32Fields msg1;
    EXPECT_TRUE(msg1.ParseFromString(msg2.SerializeAsString()));
    EXPECT_THAT(GetUnknownFieldsType(msg1), IsEmpty());
    // The encoding just appears misinterpted.
    EXPECT_THAT(msg1, EqualsProto("f32s: 100 f32s: 0"));
  }
}

// Parameter is the source protobuf to be parsed by another message.
class ProtobufRepeatedOptFilesTest : public ProtobufsAliasingTest,
                                     public ::testing::WithParamInterface<RepeatedFields> {};

TEST_P(ProtobufRepeatedOptFilesTest, RepeatedToOptionalRejected) {
  std::string data = GetParam().SerializeAsString();

  auto msg2 = db_->GetMessage("experimental.OptionalFields");
  EXPECT_TRUE(msg2->ParseFromString(data));

  msg2->pl_allow_repeated_opt_fields_ = false;
  EXPECT_FALSE(msg2->ParseFromString(data));
  EXPECT_FALSE(msg2->ParseFromArray(data.data(), data.size()));
}

RepeatedFields ParseRepeatedFieldsText(const std::string& text) {
  RepeatedFields res;
  TextFormat::ParseFromString(text, &res);
  return res;
}

INSTANTIATE_TEST_SUITE_P(
    DifferentElementType, ProtobufRepeatedOptFilesTest,
    ::testing::Values(ParseRepeatedFieldsText("i32s: 100 i32s: 100"),
                      ParseRepeatedFieldsText("strs: 'test' strs: 'test'"),
                      ParseRepeatedFieldsText("enums: V1 enums: V2"),
                      ParseRepeatedFieldsText("foo1s { i32: 100 } foo1s { i32: 100 }")));

TEST(ProtobufBasicTest, StringFieldCanContainNonUTF8) {
  std::string all_chars;
  for (int c = std::numeric_limits<char>::min(); c <= std::numeric_limits<char>::max(); ++c) {
    all_chars.push_back(c);
  }
  String str;
  str.set_str(all_chars);
  std::string str_serilized;
  // Other languages might be stricter than C++, which only throws a warning in the log.
  EXPECT_TRUE(str.SerializeToString(&str_serilized));
}

TEST(ProtobufBasicTest, StringToRpeatedI32) {
  {
    String str;
    // This is the bytes for a varint-encoded number 300.
    str.set_str("\xAC\x02");
    RepeatedInt32 ri32;
    EXPECT_TRUE(ri32.ParseFromString(str.SerializeAsString()));
    EXPECT_THAT(ri32, EqualsProto("ri32s: 300"));
  }
  {
    String str;
    // This is an invalid varint, as it should have additional trailing bytes.
    str.set_str("\xAC");
    RepeatedInt32 ri32;
    EXPECT_FALSE(ri32.ParseFromString(str.SerializeAsString()));
  }
}

TEST(IsMoreSpecializedTest, ResultsAreAsExpected) {
  {
    // Prefers message field.
    MessageWithLengthDelimitedFields1 msg1;
    msg1.mutable_msg()->set_i32(100);

    MessageWithLengthDelimitedFields2 msg2;
    EXPECT_TRUE(msg2.ParseFromString(msg1.SerializeAsString()));
    EXPECT_EQ(-1, IsMoreSpecialized(msg1, msg2));
  }
  {
    // Identical message returns 0.
    MessageWithLengthDelimitedFields1 msg;
    msg.mutable_msg()->set_i32(100);
    EXPECT_EQ(0, IsMoreSpecialized(msg, msg));
  }
}

TEST(GetAliasingFieldPairsTest, IdenticalFieldsAreReturned) {
  Foo1 foo1;
  DescriptorProto desc_pb1;
  foo1.GetDescriptor()->CopyTo(&desc_pb1);

  Foo2 foo2;
  DescriptorProto desc_pb2;
  foo2.GetDescriptor()->CopyTo(&desc_pb2);

  std::vector<AliasingFieldPair> res = GetAliasingFieldPairs(desc_pb1, desc_pb2);
  ASSERT_THAT(res, SizeIs(1));
  const AliasingFieldPair& pair = res[0];
  EXPECT_THAT(pair, EqualsProto(R"proto(
      first {
        name: "i32"
        number: 1
        label: LABEL_OPTIONAL
        type: TYPE_INT32
      }
      second {
        name: "i32"
        number: 1
        label: LABEL_OPTIONAL
        type: TYPE_INT32
      }
      direction: BIDRECTIONAL
  )proto"));
}

TEST(GetAliasingFieldPairsTest, FromTestProto) {
  std::vector<std::vector<AliasingFieldPair>> aliasing_pairs;
  FileDescriptorProto file_desc;
  Foo1::descriptor()->file()->CopyTo(&file_desc);
  for (const DescriptorProto& msg1 : file_desc.message_type()) {
    for (const DescriptorProto& msg2 : file_desc.message_type()) {
      if (&msg1 == &msg2) {
        continue;
      }
      aliasing_pairs.push_back(GetAliasingFieldPairs(msg1, msg2));
    }
  }
}

TEST(GetAliasingFieldPairsTest, ForMessages) {
  DescriptorProto msg1, msg2;
  TextFormat::ParseFromString(R"proto(
          name: "msg1"
          field {
            name: "query"
            number: 1
            label: LABEL_OPTIONAL
            type: TYPE_MESSAGE
            type_name: "nested_msg1"
          }
          )proto",
                              &msg1);
  TextFormat::ParseFromString(R"proto(
          name: "msg2"
          field {
            name: "query"
            number: 1
            label: LABEL_OPTIONAL
            type: TYPE_MESSAGE
            type_name: "nested_msg2"
          }
          )proto",
                              &msg2);
  EXPECT_THAT(GetAliasingFieldPairs(msg1, msg2), IsEmpty());

  msg1.mutable_field(0)->set_type_name("nested_msg");
  msg2.mutable_field(0)->set_type_name("nested_msg");
  EXPECT_THAT(GetAliasingFieldPairs(msg1, msg2), ElementsAre(EqualsProto(R"proto(
      first: {
        name: "query"
        number: 1
        label: LABEL_OPTIONAL
        type: TYPE_MESSAGE
        type_name: "nested_msg"
      }
      second: {
        name: "query"
        number: 1
        label: LABEL_OPTIONAL
        type: TYPE_MESSAGE
        type_name: "nested_msg"
      }
      direction: BIDRECTIONAL
  )proto")));
}

using ::google::protobuf::compiler::DiskSourceTree;

TEST(SourceTreeTest, Test) {
  DiskSourceTree source_tree;
  const std::string rel_path = "experimental/protobufs/proto/test.proto";
  source_tree.MapPath(rel_path, ::px::testing::BazelRunfilePath(rel_path));

  SourceTreeDescriptorDatabase db(&source_tree);
  FileDescriptorProto file_desc;
  EXPECT_TRUE(db.FindFileByName(rel_path, &file_desc));
}

TEST(ComputeAliasingProbabilityTest, EqualsOne) {
  AliasingMethodPair pair;
  TextFormat::ParseFromString(R"proto(
      method1 {
        name: "cockroach.server.serverpb.Admin.Health"
        req {
          name: "cockroach.server.serverpb.HealthRequest"
        }
        resp {
          name: "cockroach.server.serverpb.HealthResponse"
        }
      }
      method2 {
        name: "cockroach.server.serverpb.Admin.QueryPlan"
        req {
          name: "cockroach.server.serverpb.QueryPlanRequest"
          field {
            name: "query"
            number: 1
            label: LABEL_OPTIONAL
            type: TYPE_STRING
          }
        }
        resp {
          name: "cockroach.server.serverpb.QueryPlanResponse"
          field {
            name: "distsql_physical_query_plan"
            number: 1
            label: LABEL_OPTIONAL
            type: TYPE_STRING
            options {
              uninterpreted_option {
                name {
                  name_part: "gogoproto.customname"
                  is_extension: true
                }
                string_value: "DistSQLPhysicalQueryPlan"
              }
            }
          }
        }
      }
  )proto",
                              &pair);
  ComputeAliasingProbability(&pair);
  EXPECT_EQ(0.25, pair.p());
}

}  // namespace
}  // namespace protobufs
}  // namespace experimental
