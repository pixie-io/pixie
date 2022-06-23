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
import fire


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
            FieldType.bit: "ExtractBool()",
            FieldType.octet: "ExtractUInt8()",
            FieldType.short: "ExtractUInt16()",
            FieldType.long: "ExtractUInt32()",
            FieldType.longlong: "ExtractUInt64()",
            FieldType.shortstr: "ExtractString()",
            FieldType.longstr: "ExtractString()",
            FieldType.table: "ExtractString()",
            FieldType.timestamp: "ExtractTimestamp()",
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

    def __post_init__(self):
        self.c_field_name = self.field_name.replace("-", "_")


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
            synchronous=1,
            fields=self.class_fields,
        )


class CodeGenerator:
    """
    Parses and generates strings that represent the different classes, methods, and fields
    """

    def __init__(self, xml_file="amqp0-9-1.xml"):
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

            amqp_classes.append(
                AMQPClass(
                    class_name=class_name,
                    class_id=class_id,
                    methods=method_structs,
                    class_fields=class_fields,
                )
            )
        return amqp_classes


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
        xml_file="amqp0-9-1.xml",
        generation_dir="generated_files",
        gen_template_dir="gen_templates",
    ):
        self.generation_dir = generation_dir
        self.template_dir = Path(gen_template_dir)
        #  In order to prevent long strings like licenses, Jinja2 is used to render the files.
        #  Jinja2 is a template rendering engine(https://pypi.org/project/Jinja2/)
        self.env = Environment(loader=FileSystemLoader(self.template_dir))
        os.makedirs(self.generation_dir, exist_ok=True)
        self.generator = CodeGenerator(xml_file)
        self.types_gen_header_path = Path(self.generation_dir) / Path("types_gen.h")
        self.struct_gen_header_path = Path(self.generation_dir) / Path("decode.h")
        self.decode_gen_path = Path(self.generation_dir) / Path("decode.cc")

    def write_type_gen_header(self):
        """
        Writes the general constants and types to types_gen.h
        """
        pass

    def write_struct_declr(self):
        """
        Writes the struct declarations for decoding to decode.h
        """
        pass

    def write_buffer_decode(self):
        """
        Writes the buffer decoding in decoding.cc
        """
        pass

    def write_all(self):
        """
        Writes all files required for code generator
        """
        pass

    def format_all(self):
        """
        Runs clang-format to format outputted c code
        """
        pass


if __name__ == "__main__":
    fire.Fire(CodeGeneratorWriter)
