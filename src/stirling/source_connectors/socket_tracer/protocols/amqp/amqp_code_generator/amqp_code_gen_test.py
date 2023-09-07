# Copyright 2018- The Pixie Authors.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# SPDX-License-Identifier: Apache-2.0

from unittest import TestCase, main
import xml.etree.ElementTree as ET
from px.src.stirling.source_connectors.socket_tracer.protocols.amqp.amqp_code_generator.amqp_code_gen import (
    CodeGenerator,
    Field,
    AMQPMethod,
    AMQPClass,
    FieldType,
)


def sanitize_code_gen_output(str_input):
    """
    For code generated input, it's easier to compare the string without worrying about whitespace.
    """
    return (
        str_input.strip()
        .replace(" ", "")
        .replace("\n", "")
        .replace("\t", "")
        .replace("\r", "")
    )


class BaseTestCase(TestCase):
    def assertEqualGenStr(self, expected, result):
        self.assertEqual(
            sanitize_code_gen_output(expected), sanitize_code_gen_output(result)
        )


class TestAMQPField(BaseTestCase):
    def test_gen_field_declr(self):
        sample_field = Field("sample_field", FieldType.long)
        self.assertEqual(sample_field.gen_field_declr(), "uint32_t sample_field = 0;")

    def test_gen_json_builder(self):
        sample_field = Field("sample_field", FieldType.longstr)
        self.assertEqual(
            sample_field.gen_json_builder(),
            'builder->WriteKV("sample_field", sample_field);',
        )

    def test_gen_buffer_extract_short(self):
        sample_field = Field("sample_field", FieldType.short)
        self.assertEqual(
            sample_field.gen_buffer_extract(),
            "PX_ASSIGN_OR_RETURN(r.sample_field, decoder->ExtractBEInt<uint16_t>());",
        )

    def test_gen_buffer_extract_bit(self):
        sample_field = Field("sample_field", FieldType.bit)
        self.assertEqual(
            sample_field.gen_buffer_extract_bit(3),
            "PX_ASSIGN_OR_RETURN(r.sample_field, ExtractNthBit(decoder, 3));",
        )

    def test_get_class_buffer_extract(self):
        sample_field = Field("sample_field", FieldType.octet)
        self.assertEqual(
            sample_field.get_class_buffer_extract(5),
            """
            if((property_flags >> 5) & 1) {
                PX_ASSIGN_OR_RETURN(r.sample_field, decoder->ExtractChar<uint8_t>());
            }
            """,
        )


class TestAMQPMethod(BaseTestCase):
    def setUp(self) -> None:
        self.amqp_method_empty = AMQPMethod(
            synchronous=1,
            class_id=0,
            method_id=1,
            fields=[],
            class_name="sample_class",
            method_name="sample_method",
        )
        self.amqp_method = AMQPMethod(
            synchronous=1,
            class_id=0,
            method_id=1,
            fields=[Field("sample_field", FieldType.timestamp)],
            class_name="sample_class",
            method_name="sample_method",
        )

    def test_gen_struct_declr_empty_fields(self):
        self.assertEqualGenStr(
            self.amqp_method_empty.gen_struct_declr(),
            """
                struct AMQPSampleClassSampleMethod {
                    bool synchronous = 1;
                    void ToJSON([[maybe_unused]] utils::JSONObjectBuilder* builder) const {

                    }
                };
            """,
        )

    def test_gen_struct_declr_with_fields(self):
        self.assertEqualGenStr(
            self.amqp_method.gen_struct_declr(),
            """
            struct AMQPSampleClassSampleMethod {
                time_t sample_field = 0;
                bool synchronous = 1;
                void ToJSON( utils::JSONObjectBuilder* builder) const {
                    builder->WriteKV("sample_field", sample_field);
                }
            };
            """,
        )

    def test_gen_buffer_extract_empty_fields(self):
        self.assertEqualGenStr(
            self.amqp_method_empty.gen_buffer_extract(),
            """
            Status ExtractAMQPSampleClassSampleMethod([[maybe_unused]] BinaryDecoder* decoder, Frame* frame) {
                AMQPSampleClassSampleMethod r;

                frame->msg = ToString(r);
                frame->synchronous = 1;
                return Status::OK();
            }
            """,
        )

    def test_get_class_buffer_extract(self):
        self.assertEqualGenStr(
            self.amqp_method.get_class_buffer_extract(),
            """
            Status ExtractAMQPSampleClassSampleMethod(BinaryDecoder* decoder, Frame* frame) {
                AMQPSampleClassSampleMethod r;
                PX_ASSIGN_OR_RETURN(r.body_size, decoder->ExtractBEInt<uint64_t>());
                PX_ASSIGN_OR_RETURN(uint16_t property_flags, decoder->ExtractBEInt<uint16_t>());
                r.property_flags = property_flags;

                frame->msg = ToString(r);
                frame->synchronous = 1;
                return Status::OK();
            }
            """,
        )

    def test_gen_method_enum_declr(self):
        self.assertEqualGenStr(
            self.amqp_method.gen_method_enum_declr(), "kAMQPSampleClassSampleMethod = 1"
        )

    def test_gen_method_enum_select_case(self):
        self.assertEqualGenStr(
            self.amqp_method.gen_method_enum_select_case(
                "Connection"
            ),  # TODO PASS CLASS NAME HERE
            """
            case Connection::kAMQPSampleClassSampleMethod:
                return ExtractAMQPSampleClassSampleMethod(decoder, req);
            """,
        )


class TestAMQPClass(BaseTestCase):
    def setUp(self) -> None:
        self.sample_class_fields = [
            Field("timestamp", FieldType.timestamp),
            Field("sample_long", FieldType.long),
            Field("sample_long_str", FieldType.longstr),
        ]
        self.sample_method_field = [Field("method_field", FieldType.timestamp)]
        sample_method = AMQPMethod(
            class_id=0,
            method_id=1,
            class_name="SampleClass",
            method_name="sample_method",
            synchronous=1,
            fields=self.sample_method_field,
        )

        self.sample_class = AMQPClass(
            class_id=0,
            class_name="SampleClass",
            methods=[sample_method],
            class_fields=self.sample_class_fields,
        )

    def test_gen_method_enum_declrs(self):
        self.assertEqualGenStr(
            self.sample_class.gen_method_enum_declrs(),
            """
            enum AMQPSampleClassMethods : uint8_t {
                kAMQPSampleclassSampleMethod = 1
            };
            """,
        )

    def test_gen_method_select(self):
        self.assertEqualGenStr(
            self.sample_class.gen_method_select(),
            """
            Status ProcessSampleClass(BinaryDecoder *decoder, Frame *req, uint16_t  method_id) {
                switch(static_cast<AMQPSampleClassMethods>(method_id)) {
                    case AMQPSampleClassMethods::kAMQPSampleclassSampleMethod:
                        return ExtractAMQPSampleclassSampleMethod(decoder, req);
                    default:
                        VLOG(1)<<absl::Substitute("Invalid Sample Class frame method $0", method_id);
                }
                return Status::OK();
            }
            """,
        )

    def test_gen_class_enum_declr(self):
        self.assertEqual(self.sample_class.gen_class_enum_declr(), "kSampleClass = 0")

    def test_gen_content_header_enum_select(self):
        self.assertEqualGenStr(
            self.sample_class.gen_content_header_enum_select(),
            """
            case AMQPClasses::kSampleClass:
                return ExtractAMQPSampleclassContentHeader(decoder, req);
            """,
        )

    def test_gen_class_enum_select_case(self):
        self.assertEqualGenStr(
            self.sample_class.gen_class_enum_select_case(),
            """
            case AMQPClasses::kSampleClass:
                return ProcessSampleClass(decoder, req, method_id);
            """,
        )


class TestAMQPCodeGeneratorParse(BaseTestCase):
    def setUp(self) -> None:
        self.code_generator = CodeGenerator()

    def test_parse_constants(self):
        xml_input = ET.fromstring(
            """
            <amqp major="0" minor="9" revision="1" port="5672">
                <constant name="frame-method" value="1"/>
                <constant name="frame-header" value="2"/>
                <constant name="frame-body" value="3"/>
                <constant name="frame-heartbeat" value="8"/>
                <constant name="frame-min-size" value="4096"/>
                <constant name="frame-end" value="206"/>
            </amqp>
            """
        )
        constants = self.code_generator.parse_constants(xml_input)
        self.assertEqual(
            constants,
            {
                "frame-method": "1",
                "frame-header": "2",
                "frame-body": "3",
                "frame-heartbeat": "8",
                "frame-min-size": "4096",
                "frame-end": "206",
            },
        )

    def test_parse_domains(self):
        xml_input = ET.fromstring(
            """
        <amqp major="0" minor="9" revision="1" port="5672">
            <domain name="class-id" type="short"/>
            <domain name="consumer-tag" type="shortstr"/>
            <domain name="delivery-tag" type="longlong"/>
            <domain name="peer-properties" type="table"/>
        </amqp>
        """
        )
        domains = self.code_generator.parse_domains(xml_input)
        self.assertEqual(
            domains,
            {
                "class-id": {"type": FieldType.short, "assert": []},
                "consumer-tag": {"type": FieldType.shortstr, "assert": []},
                "delivery-tag": {"type": FieldType.longlong, "assert": []},
                "peer-properties": {"type": FieldType.table, "assert": []},
            },
        )

    def test_process_fields(self):
        xml_input = ET.fromstring(
            """
            <method name="start" synchronous="1" index="10">
                <chassis name="client" implement="MUST"/>
                <response name="start-ok"/>
                <field name="version-major" domain="octet"/>
                <field name="version-minor" domain="octet"/>
                <field name="server-properties" domain="peer-properties"/>
                <field name="mechanisms" domain="longstr">
                    <assert check="notnull"/>
                </field>
                <field name="locales" domain="longstr">
                    <assert check="notnull"/>
                </field>
            </method>
            """
        )
        fields = self.code_generator.process_fields(xml_input)
        self.assertEqual(
            fields,
            [
                Field(field_name="version-major", field_type=FieldType.octet),
                Field(field_name="version-minor", field_type=FieldType.octet),
                Field(field_name="server-properties", field_type=FieldType.table),
                Field(field_name="mechanisms", field_type=FieldType.longstr),
                Field(field_name="locales", field_type=FieldType.longstr),
            ],
        )

    def test_parse_methods_to_structs(self):
        xml_input = ET.fromstring(
            """
            <method name="start" synchronous="1" index="10">
                <chassis name="client" implement="MUST"/>
                <response name="start-ok"/>
            </method>
            """
        )
        class_name = "connection"
        class_id = "60"
        self.assertEqual(
            self.code_generator.parse_methods_to_structs(
                xml_input, class_id, class_name
            ),
            AMQPMethod(
                class_id="60",
                class_name="connection",
                method_id="10",
                method_name="start",
                synchronous="1",
                fields=[],
            ),
        )

    def test_parse_amqp_classes(self):
        xml_input = ET.fromstring(
            """
            <class name="connection" handler="connection" index="10">
                <chassis name="server" implement="MUST"/>
                <chassis name="client" implement="MUST"/>
                <method name="start" synchronous="1" index="10">
                    <chassis name="client" implement="MUST"/>
                    <response name="start-ok"/>
                    <field name="version-major" domain="octet"/>
                    <field name="version-minor" domain="octet"/>
                    <field name="server-properties" domain="peer-properties"/>
                    <field name="mechanisms" domain="longstr">
                        <assert check="notnull"/>
                    </field>
                    <field name="locales" domain="longstr">
                        <assert check="notnull"/>
                    </field>
                </method>
            </class>
            """
        )
        self.assertEqual(
            self.code_generator.parse_amqp_classes(xml_input),
            [
                AMQPClass(
                    class_id="10",
                    class_name="Connection",
                    methods=[
                        AMQPMethod(
                            class_id="10",
                            class_name="Connection",
                            method_id="10",
                            method_name="start",
                            synchronous="1",
                            fields=[
                                Field(
                                    field_name="version-major",
                                    field_type=FieldType.octet,
                                    c_field_name="version_major",
                                    class_property_field=False,
                                ),
                                Field(
                                    field_name="version-minor",
                                    field_type=FieldType.octet,
                                    c_field_name="version_minor",
                                    class_property_field=False,
                                ),
                                Field(
                                    field_name="server-properties",
                                    field_type=FieldType.table,
                                    c_field_name="server_properties",
                                    class_property_field=False,
                                ),
                                Field(
                                    field_name="mechanisms",
                                    field_type=FieldType.longstr,
                                    c_field_name="mechanisms",
                                    class_property_field=False,
                                ),
                                Field(
                                    field_name="locales",
                                    field_type=FieldType.longstr,
                                    c_field_name="locales",
                                    class_property_field=False,
                                ),
                            ],
                        )
                    ],
                    class_fields=[],
                    content_header_method=AMQPMethod(
                        class_id="10",
                        class_name="Connection",
                        method_id=-1,
                        method_name="content-header",
                        synchronous=0,
                        fields=[
                            Field(
                                field_name="body-size",
                                field_type=FieldType.longlong,
                                c_field_name="body_size",
                                class_property_field=False,
                            ),
                            Field(
                                field_name="property-flags",
                                field_type=FieldType.short,
                                c_field_name="property_flags",
                                class_property_field=False,
                            ),
                        ],
                    ),
                )
            ],
        )


class TestAMQPCodeGeneratorGen(BaseTestCase):
    def setUp(self) -> None:
        self.code_generator = CodeGenerator()
        single_amqp_class = [
            AMQPClass(
                class_id="10",
                class_name="Connection",
                methods=[
                    AMQPMethod(
                        class_id="10",
                        class_name="Connection",
                        method_id="10",
                        method_name="start",
                        synchronous="1",
                        fields=[],
                    ),
                ],
                class_fields=[],
            )
        ]
        self.code_generator_single_class = CodeGenerator()
        self.code_generator_single_class.amqp_classes = single_amqp_class

    def test_gen_constants_enums(self):
        self.code_generator.constants = {
            "heartbeat": "5",
            "content_body": "6",
        }
        self.assertEqualGenStr(
            self.code_generator.gen_constants_enums(),
            """
            enum class AMQPConstant : uint16_t {
                kHeartbeat = 5,
                kContentBody = 6
            };
            """,
        )

    def test_generate_class_enums(self):
        self.assertEqualGenStr(
            self.code_generator.generate_class_enums(),
            """
            enum class AMQPClasses : uint8_t {
                kConnection = 10,
                kChannel = 20,
                kExchange = 40,
                kQueue = 50,
                kBasic = 60,
                kTx = 90
            };
            """,
        )

    def test_gen_method_enum_declrs(self):
        self.assertEqualGenStr(
            self.code_generator_single_class.gen_method_enum_declrs(),
            """
            enum AMQPConnectionMethods : uint8_t {
                kAMQPConnectionStart = 10
            };
            """,
        )

    def test_gen_struct_declr(self):
        self.assertEqualGenStr(
            self.code_generator_single_class.gen_struct_declr(),
            """
            struct AMQPConnectionStart {
                bool synchronous = 1;
                void ToJSON([[maybe_unused]] utils::JSONObjectBuilder* builder) const {
                }
            };
            struct AMQPConnectionContentHeader {
                uint64_t body_size = 0;
                uint16_t property_flags = 0;
                bool synchronous = 0;
                void ToJSON( utils::JSONObjectBuilder* builder) const {
                    builder->WriteKV("body_size", body_size);
                    builder->WriteKV("property_flags", property_flags);
                }
            };
            """,
        )

    def test_gen_buffer_extract(self):
        self.assertEqualGenStr(
            self.code_generator_single_class.gen_buffer_extract(),
            """
            Status ExtractAMQPConnectionStart([[maybe_unused]] BinaryDecoder* decoder, Frame* frame) {
                AMQPConnectionStart r;
                frame->msg = ToString(r);
                frame->synchronous = 1;
                return Status::OK();
            }
            Status ExtractAMQPConnectionContentHeader(BinaryDecoder* decoder, Frame* frame) {
                AMQPConnectionContentHeader r;
                PX_ASSIGN_OR_RETURN(r.body_size, decoder->ExtractBEInt<uint64_t>());
                PX_ASSIGN_OR_RETURN(uint16_t property_flags, decoder->ExtractBEInt<uint16_t>());
                r.property_flags = property_flags;
                frame->msg = ToString(r);
                frame->synchronous = 0;
                return Status::OK();
            }
            """,
        )

    def test_gen_method_select(self):
        self.assertEqualGenStr(
            self.code_generator_single_class.gen_method_select(),
            """
            Status ProcessConnection(BinaryDecoder *decoder, Frame *req, uint16_t  method_id) {
                switch(static_cast<AMQPConnectionMethods>(method_id)) {
                    case AMQPConnectionMethods::kAMQPConnectionStart:
                        return ExtractAMQPConnectionStart(decoder, req);
                    default:
                        VLOG(1)<<absl::Substitute("Invalid Connection frame method $0", method_id);
                }
                returnStatus::OK();
            }
            """,
        )

    def test_gen_class_select(self):
        self.assertEqualGenStr(
            self.code_generator_single_class.gen_class_select(),
            """
            Status ProcessFrameMethod(BinaryDecoder* decoder, Frame* req) {
                PX_ASSIGN_OR_RETURN(uint16_t class_id, decoder->ExtractBEInt<uint16_t>());
                PX_ASSIGN_OR_RETURN(uint16_t method_id, decoder->ExtractBEInt<uint16_t>());

                req->class_id = class_id;
                req->method_id = method_id;

                switch(static_cast<AMQPClasses>(class_id)) {
                    case AMQPClasses::kConnection:
                        return ProcessConnection(decoder, req, method_id);
                    default:
                        VLOG(1)<<absl::Substitute("Unparsed frame method class $0 method $1", class_id, method_id);
                }
                return Status::OK();
            }
            """,
        )

    def test_gen_process_content_header_select(self):
        self.maxDiff = None
        self.assertEqualGenStr(
            self.code_generator_single_class.gen_process_content_header_select(),
            """
            Status ProcessContentHeader(BinaryDecoder* decoder, Frame* req) {
                PX_ASSIGN_OR_RETURN(uint16_t class_id, decoder->ExtractBEInt<uint16_t>());
                PX_ASSIGN_OR_RETURN(uint16_t weight, decoder->ExtractBEInt<uint16_t>());
                req->class_id = class_id;
                if(weight != 0) {
                    return error::Internal("AMQP content header weight should be 0");
                }
                switch(static_cast<AMQPClasses>(class_id)) {
                    case AMQPClasses::kConnection:
                        return ExtractAMQPConnectionContentHeader(decoder, req);
                    default:
                        VLOG(1)<<absl::Substitute("Unparsed content header class $0", class_id);
                }
                return Status::OK();
            }
            """,
        )


if __name__ == "__main__":
    main()
