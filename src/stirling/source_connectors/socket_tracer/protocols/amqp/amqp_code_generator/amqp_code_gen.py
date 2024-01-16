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

import os
import xml.etree.ElementTree as ET
from dataclasses import dataclass
from enum import Enum, auto
from typing import List
from pathlib import Path
from jinja2 import Environment, FileSystemLoader
import subprocess
from rules_python.python.runfiles import runfiles


def to_camel_case(text):
    s = text.replace("-", " ").replace("_", " ")
    s = s.split()
    if len(text) == 0:
        return text
    return s[0].title() + "".join(i.capitalize() for i in s[1:])


class FieldType(Enum):
    bit = auto()
    octet = auto()
    short = auto()
    long = auto()
    longlong = auto()
    shortstr = auto()
    longstr = auto()
    table = auto()
    timestamp = auto()

    @classmethod
    def names(cls):
        return cls._member_names_

    @staticmethod
    def get_c_type_name(field_type):
        filed_type_mappings = {
            FieldType.bit: "bool",
            FieldType.octet: "uint8_t",
            FieldType.short: "uint16_t",
            FieldType.long: "uint32_t",
            FieldType.longlong: "uint64_t",
            FieldType.shortstr: "std::string",
            FieldType.longstr: "std::string",
            FieldType.table: "std::string",
            FieldType.timestamp: "time_t",
        }

        return filed_type_mappings[field_type]

    @staticmethod
    def get_field_extract_function(field_type):
        extract_function_c_mapping = {
            FieldType.octet: "decoder->ExtractChar<uint8_t>()",
            FieldType.short: "decoder->ExtractBEInt<uint16_t>()",
            FieldType.long: "decoder->ExtractBEInt<uint32_t>()",
            FieldType.longlong: "decoder->ExtractBEInt<uint64_t>()",
            FieldType.shortstr: "ExtractShortString(decoder)",
            FieldType.longstr: "ExtractLongString(decoder)",
            FieldType.table: "ExtractLongString(decoder)",
            FieldType.timestamp: "decoder->ExtractBEInt<time_t>()",
        }
        return extract_function_c_mapping[field_type]

    @staticmethod
    def get_c_default_value(field_type):
        if (
            field_type == FieldType.shortstr
            or field_type == FieldType.longstr
            or field_type == FieldType.table
        ):
            return '""'
        else:
            return 0


@dataclass
class Field:
    """
    Represents field xml property
    <field name="reserved-ok" domain="bit"/>.
    """

    field_name: str
    field_type: FieldType
    c_field_name: str = ""  # Represents type used in struct
    class_property_field: bool = False

    def __post_init__(self):
        self.c_field_name = self.field_name.replace("-", "_")

    def gen_field_declr(self):
        """
        The fields declared will be displayed as
        struct name {
            field_declrations
        }
        """
        c_field_type_name = FieldType.get_c_type_name(self.field_type)
        default_value = FieldType.get_c_default_value(self.field_type)
        return f"{c_field_type_name} {self.c_field_name} = {default_value};"

    def gen_json_builder(self):
        """
        The json builder will be used while computing the ToJson
        void ToJSON(utils::JSONObjectBuilder* builder) const {
            builder_statements
        }
        The field name is used as json key to be close to the key used in spec
        """
        if self.field_type == FieldType.table:
            return f"// TODO(vsrivatsa): support KV for {self.field_name} field table type"
        return f'builder->WriteKV("{self.c_field_name}", {self.c_field_name});'

    def gen_buffer_extract(self):
        """
        This will be included as list of commands in
        StatusOr<FetchReq> BinaryDecoder::ExtractFetchReq(BinaryDecoder* decoder, Request* req) {
            FetchReq r;
            PX_ASSIGN_OR_RETURN(r.replica_id, decoder->ExtractInt32());
            ...
            return r;
        }
        """
        extract_function = FieldType.get_field_extract_function(self.field_type)
        return f"PX_ASSIGN_OR_RETURN(r.{self.c_field_name}, {extract_function});"

    def gen_buffer_extract_bit(self, index):
        """
        The bit type in AMQP packs multiple values into a single octet
        """
        return f"PX_ASSIGN_OR_RETURN(r.{self.c_field_name}, ExtractNthBit(decoder, {index}));"

    def get_class_buffer_extract(self, index):
        """
        Content header fields can optionally show up based on if the property flag at an index is set.
        ex: Nth bit flag in class fields is 1, then the Nth class field will be assigned
        """
        extract_function = FieldType.get_field_extract_function(self.field_type)
        return f"""
            if((property_flags >> {index}) & 1) {{
                PX_ASSIGN_OR_RETURN(r.{self.c_field_name}, {extract_function});
            }}
            """


@dataclass
class AMQPMethod:
    """
    Represents method xml property
    <method name="start" synchronous="1" index="10" />

    The struct name used to represent the method is AMQP{class_name}{method_name}
    """

    class_id: int
    class_name: str
    method_id: int
    method_name: int
    synchronous: int
    fields: List[Field]
    c_struct_name: str = ""

    def __post_init__(self):
        class_name_cased = to_camel_case(self.class_name)
        method_name_cased = to_camel_case(self.method_name)
        self.c_struct_name = f"AMQP{class_name_cased}{method_name_cased}"

    def gen_struct_declr(self):
        field_declarations = "\n".join(
            [field.gen_field_declr() for field in self.fields]
        )
        field_json_builder = "\n".join(
            [field.gen_json_builder() for field in self.fields]
        )
        unused_attribute = "[[maybe_unused]]" if len(self.fields) == 0 else ""
        json_builder_function = f"""
            void ToJSON({unused_attribute} utils::JSONObjectBuilder* builder) const {{
                        {field_json_builder}
            }}
        """
        return f"""
            struct {self.c_struct_name} {{
                {field_declarations}
                bool synchronous = {self.synchronous};
                {json_builder_function}
            }};
        """

    def get_field_buffer_extractions(self, fields: List[Field]):
        """
        Extracts the fields from the buffer and assigns them to the struct.
        Ex:
        PX_ASSIGN_OR_RETURN(r.replica_id, decoder->ExtractInt32());

        For bit type, consecutive bits are extracted and compacted into a single octet.
        If there are more than 8 bits, it flows into the next octet.

        The fields:
        <field name="wait" domain="bit"/>
        <field name="local" domain="bit" />
        <field name="test" domain="bit" />
        will be packed into the octet as 0b00000<test bit><local bit><wait bit>.

        """
        field_type_bit_counter = 0
        field_buffer_extractions = []
        extract_octet_str = f"{FieldType.get_field_extract_function(FieldType.octet)};"
        for field in fields:
            if field.field_type == FieldType.bit:
                if field_type_bit_counter == 8:
                    field_buffer_extractions.append(extract_octet_str)
                    field_type_bit_counter = 0
                field_buffer_extractions.append(
                    field.gen_buffer_extract_bit(field_type_bit_counter)
                )
                field_type_bit_counter += 1
            else:
                if field_type_bit_counter > 0:
                    field_buffer_extractions.append(extract_octet_str)
                    field_type_bit_counter = 0
                field_buffer_extractions.append(field.gen_buffer_extract())

        if field_type_bit_counter > 0:
            field_buffer_extractions.append(extract_octet_str)

        return "\n".join(field_buffer_extractions)

    def gen_buffer_extract(self):
        field_buffer_extractions = self.get_field_buffer_extractions(self.fields)
        unused_attribute = "[[maybe_unused]]" if len(self.fields) == 0 else ""
        return f"""
            Status Extract{self.c_struct_name}({unused_attribute} BinaryDecoder* decoder, Frame* frame) {{
                {self.c_struct_name} r;
                {field_buffer_extractions}
                frame->msg = ToString(r);
                frame->synchronous = {self.synchronous};
                return Status::OK();
            }}
        """

    def gen_method_enum_declr(self):
        """
        This will be included in a enum declr of the form:
        enum AMQPTxMethods : uint8_t {
            kAMQPTxSelect = 10,
            ...
        }
        """
        return f"k{self.c_struct_name} = {self.method_id}"

    def gen_method_enum_select_case(self, constant_enum_name):
        """
        Case used to Select the correct method to extract. The generated form will be
        switch (static_cast<AMQPChannelMethods>(method_id))
            case AMQPChannelMethods::kAMQPChannelOpen:
                return ExtractAMQPChannelOpen(decoder, req);
            ...
        """
        return f"""
            case {constant_enum_name}::k{self.c_struct_name}:
                return Extract{self.c_struct_name}(decoder, req);
        """

    def get_class_buffer_extract(self):
        """
        This extracts fields for the content header and uses the property flag field to set the relevant fields.
        The property flag is always 16 bits.

        The list of propert fields can be:
            content-type,
            content-encoding,
            delivery-mode
        If the bits 101 are set in a 3 bit property flag,
        the fields content-type and deliver-mode would have to be extracted and assigned.

        TODO: There is a special case where the property flags overflows beyonds 16 bits not handled.
        However, this is currently not handled or implemented anywhere in other decoders.
        """
        class_property_flag_len: int = 17
        field_buffer_extractions = "\n".join(
            [
                field.get_class_buffer_extract(class_property_flag_len - i)
                for i, field in enumerate(self.fields)
                if field.class_property_field
            ]
        )
        return f"""
            Status Extract{self.c_struct_name}(BinaryDecoder* decoder, Frame* frame) {{
                {self.c_struct_name} r;
                PX_ASSIGN_OR_RETURN(r.body_size, decoder->ExtractBEInt<uint64_t>());
                PX_ASSIGN_OR_RETURN(uint16_t property_flags, decoder->ExtractBEInt<uint16_t>());
                r.property_flags = property_flags;
                {field_buffer_extractions}
                frame->msg = ToString(r);
                frame->synchronous = {self.synchronous};
                return Status::OK();
            }}
        """


@dataclass
class AMQPClass:
    """
    Represents method xml property
    <class name="connection" index="10" />

    - Generates content Header class and contruction

    """

    class_id: int
    class_name: str
    methods: List[AMQPMethod]

    class_fields: List[Field]
    content_header_method: AMQPMethod = None

    def __post_init__(self):
        self.content_header_method = AMQPMethod(
            class_name=self.class_name,
            method_id=-1,
            class_id=self.class_id,
            method_name="content-header",
            synchronous=0,
            fields=[
                Field(field_name="body-size", field_type=FieldType.longlong),
                Field(field_name="property-flags", field_type=FieldType.short),
            ]
            + self.class_fields,
        )

    @property
    def constant_enum_name(self):
        return f"AMQP{self.class_name}Methods"

    def gen_method_enum_declrs(self):
        """
        Generates all enum declarations for methods of the form:
        enum AMQPBasicMethods : uint8_t {
            kAMQPBasicQos = 10,
            ...
        }
        """
        method_declaration = ",\n".join(
            [method.gen_method_enum_declr() for method in self.methods]
        )
        return f"""
            enum {self.constant_enum_name} : uint8_t {{
                {method_declaration}
            }};
            """

    def gen_content_header_enum_select(self):
        """
        This will be included to easily select the extraction methods given class_id
        switch (method_id) {
            case AMQPConnectionMethods::kAMQPConnectionStart:
                return ExtractAMQPConnectionStart(decoder, req);
        """
        return f"""
            case AMQPClasses::k{self.class_name}:
                return Extract{self.content_header_method.c_struct_name}(decoder, req);
        """

    def gen_method_select(self):
        """
        This will select the relevant method to extract from the buffer for a specific class.
        switch (static_cast<AMQPChannelMethods>(method_id))
            case AMQPChannelMethods::kAMQPChannelOpen:
                return ExtractAMQPChannelOpen(decoder, req);
            ...
        """
        method_cases = "\n".join(
            [
                method.gen_method_enum_select_case(self.constant_enum_name)
                for method in self.methods
            ]
        )
        return f"""
            Status Process{self.class_name}(BinaryDecoder *decoder, Frame *req, uint16_t  method_id) {{
                switch(static_cast<{self.constant_enum_name}>(method_id)) {{
                    {method_cases}
                    default:
                        VLOG(1) << absl::Substitute("Invalid {self.class_name} frame method $0", method_id);
                }}
                return Status::OK();
            }}
        """

    def gen_class_enum_declr(self):
        """
        This will be included as list of statements in a enum like
        enum class AMQPClasses : uint8_t {
            kConnection = 10,
            ...
        }
        """
        return f"k{self.class_name} = {self.class_id}"

    def gen_class_enum_select_case(self):
        """
        This will be included to easily select the extraction methods given class_id for Content Header Frame Type
        switch (class_id) {
            case AMQPConnectionMethods::kAMQPConnectionStart:
                return ExtractAMQPConnectionStart(decoder, req);
        """
        return f"""
            case AMQPClasses::k{self.class_name}:
                return Process{self.class_name}(decoder, req, method_id);
        """


class CodeGenerator:
    """
    Parses and generates strings that represent the different classes, methods, and fields
    """

    def __init__(self, xml_file=None):
        if not xml_file:
            bzl_base_path = "px/src/stirling/source_connectors/socket_tracer/protocols/amqp/amqp_code_generator/"
            r = runfiles.Create()
            xml_file = r.Rlocation(bzl_base_path + "amqp0-9-1.stripped.xml")
        with open(xml_file, "r") as f:
            amqp_xml = ET.fromstring(f.read())

        self.constants = self.parse_constants(amqp_xml)
        self.domains = self.parse_domains(amqp_xml)
        self.generation_dir = "gen/"
        self.amqp_classes = self.parse_amqp_classes(amqp_xml)

    def parse_constants(self, amqp_xml):
        """
        amqp_xml has a list of <constant name="<>" value="<>">.
        These are general amqp constants.
        The input is a full xml <amqp></amqp>
        """
        constants = {}
        for constant in amqp_xml.iter("constant"):
            name, value = constant.get("name"), constant.get("value")
            constants[name] = value
        return constants

    def parse_domains(self, amqp_xml):
        """
        amqp_xml has a list of <domain name="<>" type="<>">.
        These map certain properties/fields to type(such as uint8_t) representation
        """
        domains = {}
        for domain in amqp_xml.iter("domain"):
            name, dom_type, dom_assert = (
                domain.get("name"),
                domain.get("type"),
                domain.get("assert", []),
            )
            domains[name] = {"type": FieldType[dom_type], "assert": dom_assert}

        return domains

    def process_fields(self, fields_xml):
        """
        Given a class or a method, it will find all fields properties within the first child.
        <class>
            <field>
            ....
        </class>
        or
        <method>
            <field>
            ...
        </method>
        The field structure parsed is
        <field name="", domain="">
        or
        <field name="", type="">
        """
        fields = []
        for field_xml in fields_xml.findall("field"):
            field_name = field_xml.get("name")
            field_domain = field_xml.get("domain")
            field_type_str = field_xml.get("type")
            assert field_domain is not None or field_type_str is not None

            if field_type_str:
                field_type = FieldType[field_type_str]
            elif field_domain:
                field_type = self.domains[field_domain]["type"]

            fields.append(Field(field_name=field_name, field_type=field_type))
        return fields

    def parse_methods_to_structs(self, method_xml, class_id, class_name):
        """
        Converts method xml into AMQPMethod object.
        class_id is the parent classes's property.
        method_xml is of the form:
        <method index="" name="" synchronous=1>
            <field>
            ...
        </method>
        """
        method_id = method_xml.get("index")
        method_name = method_xml.get("name")
        synchronous = method_xml.get("synchronous", 0)
        fields = self.process_fields(method_xml)

        return AMQPMethod(
            class_id=class_id,
            class_name=class_name,
            method_id=method_id,
            method_name=method_name,
            synchronous=synchronous,
            fields=fields,
        )

    def parse_amqp_classes(self, amqp_xml):
        """
        Parses all AMQP xml class blocks to create a list of available classes.
        Each class has the form:
        <class>
            <method>
            ...
        </class>
        """
        amqp_classes = []
        for class_xml in amqp_xml.iter("class"):
            class_name = to_camel_case(class_xml.get("name"))
            class_id = class_xml.get("index")
            method_structs: List[AMQPMethod] = []
            for amqp_method_xml in class_xml.iter("method"):
                method_struct = self.parse_methods_to_structs(
                    amqp_method_xml, class_id=class_id, class_name=class_name
                )
                method_structs.append(method_struct)
            class_fields = self.process_fields(class_xml)
            for field in class_fields:
                field.class_property_field = True
            amqp_classes.append(
                AMQPClass(
                    class_name=class_name,
                    class_id=class_id,
                    methods=method_structs,
                    class_fields=class_fields,
                )
            )
        return amqp_classes

    def gen_constants_enums(self):
        """
        General AMQP constants
        """
        constant_declarations = ",\n".join(
            [f"k{to_camel_case(k)} = {v}" for k, v in self.constants.items()]
        )
        return f"""
            enum class AMQPConstant : uint16_t {{
                {constant_declarations}
            }};
        """

    def generate_class_enums(self):
        """
        Enum struct that holds the general types such as Connection, Basic, etc.
        enum class AMQPClasses : uint8_t {
            kConnection = 10,
            kChannel = 20,
            kExchange = 40,
            kQueue = 50,
            kBasic = 60,
            kTx = 90
        };
        """
        constant_declarations = ",\n".join(
            [amqp_class.gen_class_enum_declr() for amqp_class in self.amqp_classes]
        )
        return f"""
            enum class AMQPClasses : uint8_t {{
                {constant_declarations}
            }};
        """

    def gen_method_enum_declrs(self):
        """
        For each class, there's a list of methods that it supports.
        This method generates the enum declarations to find the relevant function from the method_id.
        enum AMQPTxMethods : uint8_t {
            kAMQPTxSelect = 10,
            kAMQPTxSelectOk = 11,
            kAMQPTxCommit = 20,
            kAMQPTxCommitOk = 21,
            kAMQPTxRollback = 30,
            kAMQPTxRollbackOk = 31
        };
        """
        return "\n".join(
            [amqp_class.gen_method_enum_declrs() for amqp_class in self.amqp_classes]
        )

    def gen_struct_declr(self):
        struct_definitions = []
        for amqp_class in self.amqp_classes:
            struct_definitions = (
                struct_definitions
                + [
                    method_struct.gen_struct_declr()
                    for method_struct in amqp_class.methods
                ]
                + [amqp_class.content_header_method.gen_struct_declr()]
            )

        return "\n".join(struct_definitions)

    def gen_contentbody_extract(self):
        return "// TOOD handle extract content body"

    def gen_contentheader_extract(self):
        return "// TODO handle extract content body"

    def gen_buffer_extract(self):
        """
        Extract the individual struct from the buffer
        """

        buffer_extract_methods = []
        for amqp_class in self.amqp_classes:
            buffer_extract_methods = (
                buffer_extract_methods
                + [
                    method_struct.gen_buffer_extract()
                    for method_struct in amqp_class.methods
                ]
                + [amqp_class.content_header_method.get_class_buffer_extract()]
            )
        return "\n".join(buffer_extract_methods)

    def gen_method_select(self):
        """
        Generates all the method selectors for the AMQPClasses
        """
        class_select_methods = []
        for amqp_class in self.amqp_classes:
            class_select_methods.append(amqp_class.gen_method_select())
        return "\n".join(class_select_methods)

    def gen_class_select(self):
        """
        Given a buffer, uses the class_id to find list of functions that can extract the buffer.
        Given the class_id, a list of relevant methods to the AMQP class can be found via method_id.
        """

        amqp_extract_class_case = []
        for amqp_class in self.amqp_classes:
            amqp_extract_class_case.append(amqp_class.gen_class_enum_select_case())

        amqp_extract_class_case_str = "\n".join(amqp_extract_class_case)
        return f"""
        Status ProcessFrameMethod(BinaryDecoder* decoder, Frame* req) {{
            PX_ASSIGN_OR_RETURN(uint16_t class_id, decoder->ExtractBEInt<uint16_t>());
            PX_ASSIGN_OR_RETURN(uint16_t method_id, decoder->ExtractBEInt<uint16_t>());

            req->class_id = class_id;
            req->method_id = method_id;

            switch(static_cast<AMQPClasses>(class_id)) {{
                {amqp_extract_class_case_str}
                default:
                    VLOG(1) << absl::Substitute("Unparsed frame method class $0 method $1", class_id, method_id);
            }}

            return Status::OK();
        }}
        """

    def gen_process_frame_type(self):
        return """
        Status ProcessPayload(Frame* req, BinaryDecoder* decoder) {
            // Extracts api_key, api_version, and correlation_id.
            AMQPFrameTypes amqp_frame_type = static_cast<AMQPFrameTypes>(req->frame_type);
            switch (amqp_frame_type) {
                case AMQPFrameTypes::kFrameHeader:
                    return ProcessContentHeader(decoder, req);
                case AMQPFrameTypes::kFrameBody: {
                    req->msg = "";
                    auto status = decoder->ExtractBufIgnore(req->payload_size);
                    if (!status.ok()) {
                        VLOG(1) << absl::Substitute("Failed to extract body for AMQP, error: $0", status.ToString());
                    }
                    break; // Ignore bytes in content body since length already provided by header
                }
                case AMQPFrameTypes::kFrameHeartbeat:
                    req->msg = "";
                    break; // Heartbeat frames have no body or length
                case AMQPFrameTypes::kFrameMethod:
                    return ProcessFrameMethod(decoder, req);
                default:
                    VLOG(1) << absl::Substitute("Unparsed frame $0", req->frame_type);
            }
            return Status::OK();
        }"""

    def gen_process_content_header_select(self):
        """
        Process the content header frame type by selecting the relevant content header extract method
        """
        amqp_extract_class_case = []
        for amqp_class in self.amqp_classes:
            amqp_class_case = amqp_class.gen_content_header_enum_select()
            amqp_extract_class_case.append(amqp_class_case)

        amqp_extract_class_case_str = "\n".join(amqp_extract_class_case)
        return f"""
        Status ProcessContentHeader(BinaryDecoder* decoder, Frame* req) {{
            PX_ASSIGN_OR_RETURN(uint16_t class_id, decoder->ExtractBEInt<uint16_t>());
            PX_ASSIGN_OR_RETURN(uint16_t weight, decoder->ExtractBEInt<uint16_t>());
            req->class_id = class_id;

            if(weight != 0) {{
                return error::Internal("AMQP content header weight should be 0");
            }}
            switch(static_cast<AMQPClasses>(class_id)) {{
                {amqp_extract_class_case_str}
                default:
                    VLOG(1) << absl::Substitute("Unparsed content header class $0", class_id);
            }}
            return Status::OK();
        }}
        """

    def gen_class_id_to_class_name(self):
        """
        Mapping from class_id to class_name for the Px script
        """
        amqp_class_id_class_name = []
        for amqp_class in self.amqp_classes:
            amqp_class_id_class_name.append(
                f"""
                    case {amqp_class.class_id}:
                        return "{amqp_class.class_name}";
                """
            )
        amqp_class_id_class_name_str = "\n".join(amqp_class_id_class_name)
        return f"""
        std::string ClassIdToClassName(uint16_t class_id) {{
            switch(class_id) {{
                {amqp_class_id_class_name_str}
                default:
                    return "Unknown";
            }}
        }}
        """

    def gen_method_id_to_method_name(self):
        """
        Mapping from class_id and method_id to name.
        ex. Class: Connection, Method: Start => ConnectionStart
        """
        amqp_class_id_method_name = []
        for amqp_class in self.amqp_classes:
            for method in amqp_class.methods:
                amqp_class_name = amqp_class.class_name.capitalize()
                amqp_method_name = method.method_name.capitalize()
                amqp_class_id_method_name.append(
                    f"""
                        if(method_id == {method.method_id} && class_id == {amqp_class.class_id}) {{
                            return "{amqp_class_name}{amqp_method_name}";
                        }}
                    """
                )
        amqp_class_id_method_name_str = "\n".join(amqp_class_id_method_name)
        return f"""
        std::string ClassIdMethodIdToMethodName(uint16_t class_id, uint16_t method_id) {{
            if(class_id != 0 && method_id == 0) {{
                return ClassIdToClassName(class_id);
            }}
            {amqp_class_id_method_name_str}
            return "Unknown";
        }}
        """


class CodeGeneratorWriter:
    """
    Uses the AMQP Spec to generate header/c++ files for decoding.

    Parses the xml document and generates files to support parsing of the types.
    The generated files are:
    types_gen.h - enum declarations
    struct_gen.h - struct declarations
    struct_gen.cc - struct extractions
    """

    def __init__(
        self,
        xml_file="amqp0-9-1.stripped.xml",
        generation_dir="generated_files",
        gen_template_dir="gen_templates",
    ):
        bzl_base_path = "px/src/stirling/source_connectors/socket_tracer/protocols/amqp/amqp_code_generator/"
        r = runfiles.Create()
        xml_file = r.Rlocation(bzl_base_path + xml_file)
        full_base_path = os.path.dirname(xml_file)

        self.generation_dir = os.path.join(full_base_path, generation_dir)
        self.template_dir = Path(os.path.join(full_base_path, gen_template_dir))
        os.makedirs(self.generation_dir, exist_ok=True)
        #  In order to prevent long strings like licenses, Jinja2 is used to render the files.
        #  Jinja2 is a template rendering engine(https://pypi.org/project/Jinja2/)
        self.env = Environment(loader=FileSystemLoader(self.template_dir))
        self.generator = CodeGenerator(xml_file)
        self.types_gen_header_path = Path(self.generation_dir) / Path("types_gen.h")
        self.struct_gen_header_path = Path(self.generation_dir) / Path("decode.h")
        self.decode_gen_path = Path(self.generation_dir) / Path("decode.cc")
        self.amqp_pxl_function_gen_path = Path(self.generation_dir) / Path(
            "amqp.h"
        )

    def write_type_gen_header(self):
        """
        Writes the general constants and types to types_gen.h using the template in types_gen.h.jinja2
        Writes constants enums such as:
        enum class AMQPConstant : uint16_t {
            kFrameMethod = 1,
            kFrameHeader = 2,
        }
        Writes class enums such as:
        enum AMQPTxMethods : uint8_t {
            kAMQPTxSelect = 10,
            kAMQPTxSelectOk = 11,
            ...
        };
        Writes method enums such as:
        enum AMQPExchangeMethods : uint8_t {
            kAMQPExchangeDeclare = 10,
            kAMQPExchangeDeclareOk = 11,
            ...
        };
        """
        constant_enums = self.generator.gen_constants_enums()
        class_enums = self.generator.generate_class_enums()
        method_enums = self.generator.gen_method_enum_declrs()

        template = self.env.get_template("types_gen.h.jinja_template")

        with self.types_gen_header_path.open("w") as f:
            f.write(
                template.render(
                    constant_enums=constant_enums,
                    class_enums=class_enums,
                    method_enums=method_enums,
                )
            )

    def write_struct_declr(self):
        """
        Writes the struct declarations for decoding to decode.h
        """
        struct_declr = self.generator.gen_struct_declr()
        template = self.env.get_template("decode.h.jinja_template")
        with self.struct_gen_header_path.open("w") as f:
            f.write(template.render(struct_declr=struct_declr))

    def write_buffer_decode(self):
        """
        Writes the buffer decoding in decoding.cc
        """
        process_class_methods = self.generator.gen_buffer_extract()
        process_content_header = self.generator.gen_process_content_header_select()

        process_method_type = self.generator.gen_method_select()
        process_class_type = self.generator.gen_class_select()
        process_frame_type = self.generator.gen_process_frame_type()

        template = self.env.get_template("decode.cc.jinja_template")

        with self.decode_gen_path.open("w") as f:
            f.write(
                template.render(
                    process_content_header=process_content_header,
                    process_frame_type=process_frame_type,
                    process_class_type=process_class_type,
                    process_method_type=process_method_type,
                    process_class_methods=process_class_methods,
                )
            )

    def write_px_script_functions(self):
        class_id_class_name = self.generator.gen_class_id_to_class_name()
        method_id_method_name = self.generator.gen_method_id_to_method_name()
        template = self.env.get_template("amqp_pxl_function.h.jinja_template")

        with self.amqp_pxl_function_gen_path.open("w") as f:
            f.write(
                template.render(
                    class_id_class_name=class_id_class_name,
                    method_id_method_name=method_id_method_name,
                )
            )

    def run(self):
        """
        Runs code generation for AMQP.
        This uses the parsed types from AMQP specification xml_file.
        Then, writes the parsed logic to types_gen.h, decode.h, decode.cc
        """
        self.write_type_gen_header()
        self.write_struct_declr()
        self.write_buffer_decode()
        self.write_px_script_functions()
        self.format_all()

    def format_all(self):
        """
        Runs clang-format to format outputted c code
        """
        if input("Use clang-format to format code[y/n]") != "y":
            return

        p = subprocess.Popen(
            [
                "clang-format",
                "-style=Google",
                "-i",
                str(self.struct_gen_header_path),
                str(self.decode_gen_path),
                str(self.types_gen_header_path),
                str(self.amqp_pxl_function_gen_path),
            ]
        )
        p.wait()
